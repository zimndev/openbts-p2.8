/*
* Copyright 2008, 2009, 2010 Free Software Foundation, Inc.
* Copyright 2010 Kestrel Signal Processing, Inc.
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



#include "Transceiver.h"
#include "radioDevice.h"
#include "DummyLoad.h"

#include <time.h>
#include <signal.h>

#include <GSMCommon.h>
#include <Logger.h>
#include <Configuration.h>

using namespace std;

ConfigurationKeyMap getConfigurationKeys();
ConfigurationTable gConfig("/etc/OpenBTS/OpenBTS.db", 0, getConfigurationKeys());

volatile bool gbShutdown = false;
static void ctrlCHandler(int signo)
{
   cout << "Received shutdown signal" << endl;;
   gbShutdown = true;
}


int main(int argc, char *argv[])
{
  std::string deviceArgs;

  if (argc == 3)
  {
    deviceArgs = std::string(argv[2]);
  }
  else
  {
    deviceArgs = "";
  }

  if ( signal( SIGINT, ctrlCHandler ) == SIG_ERR )
  {
    cerr << "Couldn't install signal handler for SIGINT" << endl;
    exit(1);
  }

  if ( signal( SIGTERM, ctrlCHandler ) == SIG_ERR )
  {
    cerr << "Couldn't install signal handler for SIGTERM" << endl;
    exit(1);
  }
  // Configure logger.
  gLogInit("transceiver",gConfig.getStr("Log.Level").c_str(),LOG_LOCAL7);

  int numARFCN=1;

  LOG(NOTICE) << "starting transceiver with " << numARFCN << " ARFCNs (argc=" << argc << ")";

  srandom(time(NULL));

  RadioDevice *usrp = RadioDevice::make(SAMPSPERSYM);
  int radioType = usrp->open(deviceArgs);
  if (radioType < 0) {
    LOG(ALERT) << "Transceiver exiting..." << std::endl;
    return EXIT_FAILURE;
  }

  RadioInterface* radio;
  switch (radioType) {
  case RadioDevice::NORMAL:
    radio = new RadioInterface(usrp, 3, SAMPSPERSYM, false);
    break;
  case RadioDevice::RESAMP:
    radio = new RadioInterfaceResamp(usrp, 3, SAMPSPERSYM, false);
    break;
  default:
    LOG(ALERT) << "Unsupported configuration";
    return EXIT_FAILURE;
  }

  Transceiver *trx = new Transceiver(gConfig.getNum("TRX.Port"),gConfig.getStr("TRX.IP").c_str(),SAMPSPERSYM,GSM::Time(3,0),radio);
  trx->receiveFIFO(radio->receiveFIFO());
  trx->start();

  while (!gbShutdown) {
    sleep(1);
  }

  cout << "Shutting down transceiver..." << endl;

  delete trx;
}

ConfigurationKeyMap getConfigurationKeys()
{
	ConfigurationKeyMap map;
	ConfigurationKey *tmp;

	tmp = new ConfigurationKey("TRX.RadioFrequencyOffset","128",
		"~170Hz steps",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"96:160",// educated guess
		true,
		"Fine-tuning adjustment for the transceiver master clock.  "
			"Roughly 170 Hz/step.  "
			"Set at the factory.  "
			"Do not adjust without proper calibration."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	tmp = new ConfigurationKey("TRX.TxAttenOffset","0",
		"dB of attenuation",
		ConfigurationKey::FACTORY,
		ConfigurationKey::VALRANGE,
		"0:100",// educated guess
		true,
		"Hardware-specific gain adjustment for transmitter, matched to the power amplifier, expessed as an attenuationi in dB.  "
			"Set at the factory.  "
			"Do not adjust without proper calibration."
	);
	map[tmp->getName()] = *tmp;
	delete tmp;

	return map;
}
