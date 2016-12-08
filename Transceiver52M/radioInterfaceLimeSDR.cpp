/*
* Copyright 2008, 2009 Free Software Foundation, Inc.
*
* This software is distributed under the terms of the GNU Affero Public License.
* See the COPYING file in the main directory for details.
*
* This use of this software may be subject to additional restrictions.
* See the LEGAL file in the main directory for details.

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU Affero General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Affero General Public License for more details.

	You should have received a copy of the GNU Affero General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "radioInterface.h"
#include "Resampler.h"
#include <Logger.h>

extern "C" {
#include "convert.h"
}

#define CHUNK		625
#define NUMCHUNKS	4

static Resampler *upsampler = NULL;
static Resampler *dnsampler = NULL;

RadioInterfaceLimeSDR::RadioInterfaceLimeSDR(RadioDevice *wRadio, size_t tx_sps,
                               size_t rx_sps, size_t chans)
  : RadioInterface(wRadio, tx_sps, rx_sps, chans)
{
}

RadioInterfaceLimeSDR::~RadioInterfaceLimeSDR(void)
{
  close();
}

bool RadioInterfaceLimeSDR::init(int type)
{
  close();
  sendBuffer.resize(mChans);
  recvBuffer.resize(mChans);
  convertSendBuffer.resize(mChans);
  convertRecvBuffer.resize(mChans);
  mReceiveFIFO.resize(mChans);
  powerScaling.resize(mChans);

  for (size_t i = 0; i < mChans; i++) {
    sendBuffer[i] = new signalVector(CHUNK * mSPSTx);
    recvBuffer[i] = new signalVector(NUMCHUNKS * CHUNK * mSPSRx);

    convertSendBuffer[i] = new short[sendBuffer[i]->size() * 6];
    convertRecvBuffer[i] = new short[recvBuffer[i]->size() * 6];

    powerScaling[i] = 1.0;
  }

    const int resamp_inrate = 1;
    const int resamp_outrate = 3;
    dnsampler = new Resampler(resamp_inrate, resamp_outrate);
    if (!dnsampler->init()) {
            LOG(ALERT) << "Rx resampler failed to initialize";
            return false;
    }

    upsampler = new Resampler(resamp_outrate, resamp_inrate,4);
    if (!upsampler->init(0.45)) {
            LOG(ALERT) << "Tx resampler failed to initialize";
            return false;
    }
  return true;
}

void RadioInterfaceLimeSDR::close()
{
  for (size_t i = 0; i < sendBuffer.size(); i++)
    delete sendBuffer[i];

  for (size_t i = 0; i < recvBuffer.size(); i++)
    delete recvBuffer[i];

  for (size_t i = 0; i < convertSendBuffer.size(); i++)
    delete convertSendBuffer[i];

  for (size_t i = 0; i < convertRecvBuffer.size(); i++)
    delete convertRecvBuffer[i];

  sendBuffer.resize(0);
  recvBuffer.resize(0);
  convertSendBuffer.resize(0);
  convertRecvBuffer.resize(0);
}


/* Receive a timestamped chunk from the device */
void RadioInterfaceLimeSDR::pullBuffer()
{
  bool local_underrun;
  int num_recv;
  float *output;
  float tmp_buffer[CHUNK*6];

  if (recvCursor > recvBuffer[0]->size() - CHUNK)
    return;

  /* Outer buffer access size is fixed */
  num_recv = mRadio->readSamples(convertRecvBuffer,
                                 CHUNK*3,
                                 &overrun,
                                 readTimestamp,
                                 &local_underrun);
  if (num_recv != CHUNK*3) {
          LOG(ALERT) << "Receive error " << num_recv;
          return;
  }

  for (size_t i = 0; i < mChans; i++) {
    output = (float *) (recvBuffer[i]->begin() + recvCursor);
    convert_short_float(tmp_buffer, convertRecvBuffer[i], 2 * num_recv);
  }

  underrun |= local_underrun;

    int rc = dnsampler->rotate(tmp_buffer,
			       num_recv,
			       output,
			       CHUNK);

    if (rc < 0) {
            LOG(ALERT) << "Sample rate upsampling error";
    }

  readTimestamp += num_recv;
  recvCursor += CHUNK;
}

/* Send timestamped chunk to the device with arbitrary size */
void RadioInterfaceLimeSDR::pushBuffer()
{
  int num_sent;

  if (sendCursor < CHUNK)
    return;

  if (sendCursor > sendBuffer[0]->size())
    LOG(ALERT) << "Send buffer overflow";

  float tmp_buffer[CHUNK*6];

    int rc = upsampler->rotate((float *) sendBuffer[0]->begin(), sendCursor,
                           tmp_buffer, sendCursor*3);
    if (rc < 0) {
            LOG(ALERT) << "Sample rate upsampling error";
    }

  for (size_t i = 0; i < mChans; i++) {
    convert_float_short(convertSendBuffer[i],
                        (float *) tmp_buffer,
                        powerScaling[i], 2 * sendCursor*3);
  }


  /* Send the all samples in the send buffer */
  LOG(DEBUG) << "Sending " << sendCursor << " samples at " << writeTimestamp << " power scale = " << powerScaling[0];
  num_sent = mRadio->writeSamples(convertSendBuffer,
                                  sendCursor*3,
                                  &underrun,
                                  writeTimestamp);
  writeTimestamp += num_sent;
  sendCursor = 0;
}
