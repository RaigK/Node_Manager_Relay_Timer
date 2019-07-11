#pragma once
/*
* The MySensors Arduino library handles the wireless radio link and protocol
* between your home built sensors/actuators and HA controller of choice.
* The sensors forms a self healing radio network with optional repeaters. Each
* repeater and gateway builds a routing tables in EEPROM which keeps track of the
* network topology allowing messages to be routed to nodes.
*
* Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
* Copyright (C) 2013-2017 Sensnology AB
* Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
*
* Documentation: http://www.mysensors.org
* Support Forum: http://forum.mysensors.org
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*/
#ifndef SensorRelayTimer_h
#define SensorRelayTimer_h

/*
SensorRelayTimer
*/

#include <Bounce2.h>
#include <neotimer.h>


class SensorRelayTimer : public Sensor {
	char msg1[10] = { '\0' };
	char msg2[10] = { '\0' };
	String _name;
	long _delay = 0;
	int  _delay_Var = 0;
	bool _state = 0;

protected:
	Neotimer ntimer = Neotimer(_delay);
	Bounce _debouncer = Bounce();
	int8_t _button_pin = 14;
	int8_t _relay_pin = 4;
	int _old_state = 0;
	int _relay_on = 1;
	int _relay_off = 0;

public:
	SensorRelayTimer(uint8_t child_id = 0) : Sensor(-1) {
		static int _instance;
		
		_name = "Relay";
		_name += String(_instance);
		_name += String("\0");
		int str_len = _name.length() + 1;
		_name.toCharArray(msg1, str_len);
		_name = "Delays";
		_name += String(_instance);
		_name += String("\0");
		str_len = _name.length() + 1;
		_name.toCharArray(msg2, str_len);

		children.allocateBlocks(2);
		new Child(this, INT, nodeManager.getAvailableChildId(child_id), S_BINARY, V_STATUS, msg1);
		new Child(this, INT, nodeManager.getAvailableChildId(child_id), S_CUSTOM, V_VAR1, msg2);
//		MY_SERIALDEVICE.println(msg1);
//		MY_SERIALDEVICE.println(msg2);
		_instance++;

	};
	// [101] set the button's pin (default: 14)
	void setButtonPin(int8_t value) {
		_button_pin = value;
	};
	// [102] set the relay's pin (default: 4)
	void setRelayPin(int8_t value) {
		_relay_pin = value;
	};

	void setRelayTimer(int  _delay_Var) {
		_delay = _delay_Var * 1000;//timer takes ms
	}

		// what to do during setup
		void onSetup() {
		// Setup the button
		pinMode(_button_pin, INPUT_PULLUP);
		// After setting up the button, setup debouncer
		_debouncer.attach(_button_pin);
		_debouncer.interval(100);

		// Make sure relays and LED are off when starting up
#if NODEMANAGER_EEPROM == ON
		// keep track of the value in EEPROM so to restore it upon a reboot
		children.get()->setPersistValue(true);
#else
		// turn the output off by default
		setRelayTimer(0);
#endif

		// Then set relay pins in output mode
		pinMode(_relay_pin, OUTPUT);
	};

	// what to do during loop
	void onLoop(Child* child) {
#if NODEMANAGER_EEPROM == ON
		// if this is the first time running loop and a value has been restored from EEPROM, turn the output according the last value
		if (_first_run) {
//			MY_SERIALDEVICE.print("Type  ");
//			MY_SERIALDEVICE.println(child->getType());
			child->loadValue();
			setRelayTimer(child->getValueInt());
//			MY_SERIALDEVICE.print(" RelayTimer = ");
//			MY_SERIALDEVICE.println(_delay);
//			MY_SERIALDEVICE.println(child->getValueInt());
		}
#endif

		relayTimerCheck(child);
		_debouncer.update();

		// Get the update value from the button
		if (_debouncer.fell()) {
			debug(PSTR(LOG_SENSOR "Pressed "), child->getChildId());
			MY_SERIALDEVICE.print("Pressed ");
			MY_SERIALDEVICE.println(child->getChildId());
			_state = _state ? false : true;
			setRelay(child, _state);
		}

	};

	// what to do as the main task when receiving a message
	void onReceive(MyMessage* message) {
		Child* child = getChild(message->sensor);
		if (child == nullptr) return;
		switch (child->getType()) {
		case V_STATUS:
			if (message->getCommand() == C_SET) {
				// retrieve from the message the value to set
				setRelay(child, message->getInt());
				child->setValue(message->getInt());
//				MY_SERIALDEVICE.print(child->getChildId());
//				MY_SERIALDEVICE.print(" Relay = ");
//				MY_SERIALDEVICE.println(message->getInt());
			}
			if (message->getCommand() == C_REQ) {
				// return the current state
				child->setValue(_state);
			}
			break;
		case V_VAR1:
			if (message->getCommand() == C_SET) {
				// retrieve from the message the value to set
				child->setValue(message->getInt());
#if NODEMANAGER_EEPROM == ON
				child->saveValue();
#endif
				_delay_Var = child->getValueInt();
				//_delay_Var = (uint8_t)message->getInt(); //message contains sec
				setRelayTimer(_delay_Var);
				child->setValue(message->getInt());
//				MY_SERIALDEVICE.print(child->getChildId());
//				MY_SERIALDEVICE.print(" Delay = ");
//				MY_SERIALDEVICE.println(_delay);
			}
			break;
		case V_VAR2:
			if (message->getCommand() == C_SET) {
				// retrieve from the message the value to set
				//_delay = (uint8_t)message->getInt();
//				MY_SERIALDEVICE.print(child->getChildId());
//				MY_SERIALDEVICE.print(" Delay = ");
//				MY_SERIALDEVICE.println(_delay);
			}
			break;
		}
		};

#if NODEMANAGER_OTA_CONFIGURATION == ON
	// define what to do when receiving an OTA configuration request
	void onOTAConfiguration(ConfigurationRequest * request) {
		switch (request->getFunction()) {
		case 101: setButtonPin(request->getValueInt()); break;
		case 102: setRelayPin(request->getValueInt()); break;
//		case 103: setLedPin(request->getValueInt()); break;
		default: return;
		}
	};
#endif

protected:

	void setRelay(Child* child, int value) {
		if (_delay == 0 || value == false) {
			digitalWrite(_relay_pin, value ? _relay_on : _relay_off);
			
			if (child->getType() == V_STATUS) {
				_state = value;
				child->setValue(_state);
			}

		}
		if (_delay > 0 && value == true){
			ntimer.set(_delay);
			ntimer.start();
			MY_SERIALDEVICE.println("Timer Start");
			digitalWrite(_relay_pin, _relay_on);
			child->setValue(_relay_on); //update for manual set
		}
		_state = value;
	}

	void relayTimerCheck(Child* child)
	{
		if (ntimer.done()) {
			MY_SERIALDEVICE.println("Timer Done");
			setRelay(child, _relay_off);
			ntimer.reset();
		}
	}

};
#endif