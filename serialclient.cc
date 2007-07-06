/*
    This file is part of Kismet

    Kismet is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kismet is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Kismet; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "config.h"
#include "serialclient.h"

SerialClient::SerialClient() {
    fprintf(stderr, "*** SerialClient() called with no global registry reference\n");
}

SerialClient::SerialClient(GlobalRegistry *in_globalreg) : 
	NetworkClient(in_globalreg) {
    // Nothing special here
}

SerialClient::~SerialClient() {

}

int SerialClient::Connect(const char *in_remotehost, short int in_port) {
	cli_fd = open(in_remotehost, O_RDWR | O_NONBLOCK | O_NOCTTY);

	if (cli_fd < 0) {
		_MSG("SerialClient::Connect() failed to open serial device " +
			 string(in_remotehost) + ": " + string(strerror(errno)),
			 MSGFLAG_ERROR);
		return -1;
	}

	cl_valid = 1;

	read_buf = new RingBuffer(SER_RING_LEN);
	write_buf = new RingBuffer(SER_RING_LEN);

    return 1;
}

int SerialClient::GetOptions(struct termios *options) {
	tcgetattr(cli_fd, options);

	return 1;
}

int SerialClient::SetOptions(struct termios *options) {
	if (tcsetattr(cli_fd, TCSANOW, options) < 0) {
		_MSG("SerialClient::SetOptions() failed to set serial device attributes: " +
			 string(strerror(errno)), MSGFLAG_ERROR);
		return -1;
	} 

	return 1;
}

int SerialClient::ReadBytes() {
    uint8_t recv_bytes[1024];
    int ret;

    if ((ret = read(cli_fd, recv_bytes, 1024)) < 0) {
        snprintf(errstr, 1024, "Serial client fd %d read() error: %s", 
                 cli_fd, strerror(errno));
        globalreg->messagebus->InjectMessage(errstr, MSGFLAG_ERROR);
		KillConnection();
        return -1;
    }

    if (ret == 0) {
        snprintf(errstr, 1024, "Serial client fd %d socket closed.", cli_fd);
        globalreg->messagebus->InjectMessage(errstr, MSGFLAG_ERROR);
		KillConnection();
        return -1;
    }

    if (read_buf->InsertData(recv_bytes, ret) == 0) {
        snprintf(errstr, 1024, "Serial client fd %d read error, ring buffer full",
                 cli_fd);
        globalreg->messagebus->InjectMessage(errstr, MSGFLAG_ERROR);
		KillConnection();
        return -1;
    }

    return ret;
}

int SerialClient::WriteBytes() {
    uint8_t dptr[1024];
    int dlen, ret;

    write_buf->FetchPtr(dptr, 1024, &dlen);

    if ((ret = write(cli_fd, dptr, dlen)) <= 0) {
        snprintf(errstr, 1024, "Serial client: Killing client fd %d write error %s",
                 cli_fd, strerror(errno));
        globalreg->messagebus->InjectMessage(errstr, MSGFLAG_ERROR);
		KillConnection();
        return -1;
    }

    write_buf->MarkRead(ret);

    return ret;
}

