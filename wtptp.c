// SPDX-License-Identifier: Beerware
/*
 * 2018 by Marek Behun <marek.behun@nic.cz>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <math.h>
#include <endian.h>
#include "utils.h"
#include "wtptp.h"

static int wtpfd;

static void xread(void *buf, size_t size)
{
	ssize_t rd, res;
	struct pollfd pfd;

	pfd.fd = wtpfd;
	pfd.events = POLLIN;

	rd = 0;
	while (rd < size) {
		pfd.revents = 0;
		res = poll(&pfd, 1, -1);
		if (res < 0)
			die("Cannot poll: %m\n");

		res = read(wtpfd, buf + rd, size - rd);
		if (res < 0)
			die("Cannot read %zu bytes: %m", size);

		rd += res;
	}
}

static void xwrite(const void *buf, size_t size)
{
	ssize_t res;

	res = write(wtpfd, buf, size);
	if (res < 0)
		die("Cannot write %zu bytes: %m", size);
	else if (res < size)
		die("Cannot write %zu bytes: written only %zi", size, res);
}

void openwtp(const char *path)
{
	u8 buf[5];
	struct termios opts;

	wtpfd = open(path, O_RDWR | O_NOCTTY);

	if (wtpfd < 0)
		die("Cannot open %s: %m", path);

	memset(&opts, 0, sizeof(opts));
	tcgetattr(wtpfd, &opts);
	cfsetispeed(&opts, B115200);
	cfsetospeed(&opts, B115200);
	opts.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	opts.c_lflag &= ~(ICANON|ECHO|ECHOE|ISIG|ECHONL|IEXTEN);
	opts.c_oflag &= ~OPOST;
	opts.c_cc[VMIN] = 0;
	opts.c_cc[VTIME] = 0;
	opts.c_cflag &= ~(CSIZE|PARENB);
	opts.c_cflag |= CS8;
	tcsetattr(wtpfd, TCSANOW, &opts);

	xwrite("wtp\r", 4);
	xread(buf, 5);
	if (memcmp(buf, "wtp\r\n", 5))
		die("Wrong reply: \"%.*s\"", 5, buf);
}

void closewtp(void)
{
	close(wtpfd);
}

static void readresp(u8 cmd, u8 seq, u8 cid, resp_t *resp)
{
	xread(resp, 6);

	if (resp->cmd != cmd || resp->seq != seq || resp->cid != cid)
		die("Comparison fail: cmd[%02x %02x %02x] != "
		    "resp[%02x %02x %02x]", cmd, seq, cid, resp->cmd, resp->seq,
		    resp->cid);

	if (resp->len > 0)
		xread(((void *) resp) + 6, resp->len);
}

static void _sendcmd(u8 cmd, u8 seq, u8 cid, u8 flags, u32 len,
		     const void *data, resp_t *resp)
{
	u8 *buf;

	buf = xmalloc(8 + len);
	buf[0] = cmd;
	buf[1] = seq;
	buf[2] = cid;
	buf[3] = flags;

	*(u32 *) &buf[4] = htole32(len);
	if (len)
		memcpy(buf + 8, data, len);

	xwrite(buf, 8 + len);
	free(buf);

	if (resp)
		readresp(cmd, seq, cid, resp);
}

static void checkresp(resp_t *resp)
{
	if (resp->status == 0x2)
		die("Sequence error on command %02x", resp->cmd);
	else if (resp->status == 0x1)
		die("NACK on command %02x", resp->cmd);
}

static void sendcmd(u8 cmd, u8 seq, u8 cid, u8 flags, u32 len, const void *data,
		    resp_t *resp)
{
	int ismsg;
	resp_t msgresp;

	_sendcmd(cmd, seq, cid, flags, len, data, resp);

	if (!resp)
		return;

	for (ismsg = resp->flags & 3; ismsg & 1; ismsg = msgresp.flags & 3) {
		_sendcmd(0x2b, 0, cid, 0, 0, NULL, &msgresp);
		if (ismsg & 2)
			printf("Message from target: 0x%08x\n",
			       *(u32 *) msgresp.data);
		else
			printf("Message from target: \"%.*s\"\n",
			       (int) msgresp.len, msgresp.data);
	}

	checkresp(resp);
}

static void preamble(void)
{
	u8 buf[6];

	xwrite("\x00\xd3\x02\x2b", 4);
	xread(buf, 4);

	if (!memcmp(buf, "TIM-", 4)) {
		xread(buf, 5);
		xread(buf, 4);
	}

	if (memcmp(buf, "\x00\xd3\x02\x2b", 4))
		die("Wrong reply to preamble: \"%.*s\"", 4, buf);
}

static void getversion(void)
{
	static int printed;
	resp_t resp;
	u32 date;

	sendcmd(0x20, 0, 0, 0, 0, NULL, &resp);

	if (resp.len != 12)
		die("GetVersion response length = %i != 12", resp.len);

	date = *(u32 *) &resp.data[4];

	if (!printed) {
		printed = 1;

		printf("GetVersion response: stepping \"%.*s\", "
		       "date = %x/%x/%x, CPU \"%.*s\"\n", 4, &resp.data[0],
		       (date >> 24) & 0xff, (date >> 16) & 0xff, date & 0xffff,
		       4, &resp.data[8]);
	}
}

u32 selectimage(void)
{
	resp_t resp;

	preamble();
	getversion();

	sendcmd(0x26, 0, 0, 0, 0, NULL, &resp);

	if (resp.len != 4)
		die("SelectImage response length = %i != 4", resp.len);

	return *(u32 *) resp.data;
}

void sendimage(image_t *img, int fast)
{
	static int seq = 1;
	resp_t resp;
	u8 buf[4];
	u32 sent;
	double start;
	int diff;

	buf[0] = 0;
	sendcmd(0x27, 0, 0, 0, 1, buf, &resp);

	start = now();
	sent = 0;
	while (sent < img->size) {
		u32 tosend;
		int eta;

		if ((fast && !sent) || !fast) {
			*(u32 *) buf = htole32(img->size - sent);

			sendcmd(0x2a, seq, 0, fast ? 4 : 0, 4, buf, &resp);
			if (resp.len != 4)
				die("DataHeader response length = %i != 4\n",
				    resp.len);

			if (fast && !(resp.flags & 4))
				die("Fast mode not supported");

			tosend = le32toh(*(u32 *) resp.data);
		}

		if (img->size - sent < tosend)
			tosend = img->size - sent;

		if (fast)
			xwrite(img->data + sent, tosend);
		else
			sendcmd(0x22, seq, 0, 0, tosend, img->data + sent,
				&resp);

		sent += tosend;

		eta = lrint((now() - start) * (img->size - sent) / sent);
		printf("\r%u%% sent, ETA %02i:%02i", 100 * sent / img->size,
		       eta / 60, eta % 60);
		fflush(stdout);
	}

	diff = lrint(now() - start);
	printf("\r100%% sent in %02i:%02i  \n", diff / 60, diff % 60);

	if (fast) {
		readresp(0x22, seq, 0, &resp);
		checkresp(&resp);
	}

	sendcmd(0x30, 0, 0, 0, 0, NULL, &resp);
}

static inline u64 decode_char(int row, u8 c)
{
	if (c == 'U')
		return 1;
	else if (c == '?')
		return 0;
	else
		die("Error decoding character when reading OTP row %i", row);
}

void uart_otp_read(void)
{
	u8 buf[69];
	int i, j;

	xread(buf, 5);
	if (memcmp(buf, "OTP\r\n", 5))
		die("Wrong reply: \"%.*s\"", 5, buf);

	for (i = 0; i < 44; ++i) {
		u64 val;

		xread(buf, 69);

		if (buf[1] != ' ' || buf[34] != ' ' || buf[67] != '\r'
		    || buf[68] != '\n')
			die("Wrong reply when reading OTP row %i", i);

		val = 0;
		for (j = 0; j < 32; ++j) {
			val |= (u64) decode_char(i, buf[j + 2]) << (63 - j);
			val |= (u64) decode_char(i, buf[j + 35]) << (31 - j);
		}
		printf("OTP row %i %016llx %s\n", i, val,
		       decode_char(i, buf[0]) ? "locked" : "not locked");
	}
}
