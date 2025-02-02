/*
 *  Original written for LPC922:
 *  Copyright (c) 2013 Stefan Taferner <stefan.taferner@gmx.at>
 *
 *  Modified for LPC1115 ARM processor:
 *  Copyright (c) 2017 Oliver Stefan <o.stefan252@googlemail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <stdint.h>
#include <sblib/serial.h>
#include <sblib/digital_pin.h>
#include <sblib/io_pin_names.h>

#include "rm_com.h"
#include "rm_const.h"



// Maximale Anzahl Zeichen einer Nachricht vom Rauchmelder, exklusive STX und ETX
// Dekodiert brauchen je zwei Zeichen ein Byte Platz in recvBuf.
#define RECV_MAX_CHARS 12

// Buffer für eine dekodierte Nachricht vom Rauchmelder
uint8_t recvBuf[RECV_MAX_CHARS >> 1];

// Zähler für die empfangenen Zeichen vom Rauchmelder
int recvCount;


// Hilfsstring für die Umschlüsselung Zahl auf Hex-String
unsigned const char hexDigits[] = "0123456789ABCDEF";

// configure serial pins for Rauchmelder
//Serial serial(PIO3_1, PIO3_0);

/**
 * Serielle Kommunikation mit dem Rauchmelder initialisieren
 */
void rm_serial_init()
{
    recvCount = -1;
    serial.setRxPin(PIN_RX);
	serial.setTxPin(PIN_TX);
	serial.begin(9600);
}

/**
 * Prüfen ob Rauchmelder aktiv ist
 *
 * @return true wenn Rauchmelder aktiv, ansonsten false
 */
bool checkRmActivity(void)
{
    pinMode(RM_ACTIVITY_PIN, INPUT | PULL_DOWN);	//Pin als Eingang mit Pulldown Widerstand konfigurieren
	bool rmActiv = digitalRead(RM_ACTIVITY_PIN);
	pinMode(RM_ACTIVITY_PIN, INPUT);

	// falls der Rauchmelder auf die Bodenplatte gesteckt wurde, aber die Spannungsversorung noch nicht ativ ist
	if(rmActiv == RM_IS_ACTIVE && digitalRead(RM_SUPPORT_VOLTAGE_PIN) == RM_SUPPORT_VOLTAGE_OFF){
		digitalWrite(RM_SUPPORT_VOLTAGE_PIN, RM_SUPPORT_VOLTAGE_ON); // Spannungsversorgung aktivieren
	}
	return rmActiv;
}

/**
 * Ein Byte an den Rauchmelder senden.
 *
 * @param ch - das zu sendende Byte.
 */
void rm_send_byte(unsigned char ch)
{
	serial.write(ch);
}

void rm_send_ack()
{
    if (!checkRmActivity()) //prüfen, ob der Rauchmelder auf Bodenplatte gesteckt und somit aktiv ist
    {
        return;
    }
    rm_send_byte(ACK);
}

/**
 * Eine Nachricht an den Rauchmelder senden.
 *
 * Der Befehl wird als Hex String gesendet. Die Prüfsumme wird automatisch
 * berechnet und nach dem Befehl gesendet. Die gesammte Übertragung wird mit
 * STX begonnnen und mit ETX beendet.
 *
 * @param hexstr - die zu sendenden Bytes als Hex String, mit Nullbyte am Ende
 */
void rm_send_hexstr(unsigned char* hexstr)
{
    if (!checkRmActivity()) //prüfen, ob der Rauchmelder auf Bodenplatte gesteckt und somit aktiv ist
    {
        return;
    }

	unsigned char checksum = 0;
	unsigned char ch;

	rm_send_byte(STX);

	while (*hexstr)
	{
		ch = *hexstr;
		checksum += ch;
		rm_send_byte(ch);
		++hexstr;
	}

	rm_send_byte(hexDigits[checksum >> 4]);
	rm_send_byte(hexDigits[checksum & 15]);

	rm_send_byte(ETX);
}


/**
 * Einen 1 Byte Befehl an den Rauchmelder senden.
 */
void rm_send_cmd(unsigned char cmd)
{
    if (!checkRmActivity()) //prüfen, ob der Rauchmelder auf Bodenplatte gesteckt und somit aktiv ist
    {
        return;
    }

	unsigned char b, bytes[3];

	b = cmd >> 4;
	bytes[0] = hexDigits[b];

	b = cmd & 0x0f;
	bytes[1] = hexDigits[b];

	bytes[2] = 0;
	rm_send_hexstr(bytes);
}


/**
 * Ein Byte über die Serielle vom Rauchmelder empfangen.
 */
bool rm_recv_byte()
{
    uint32_t count = serial.available();
    if (count == 0)
    {
        return (false);
    }

    uint8_t idx;
	uint8_t ch;

	int rec_ch = serial.read();
	while (rec_ch >  -1) {
		ch = (unsigned char) rec_ch;

        // Am Anfang auf das Start Byte warten
        if (recvCount < 0)
        {
            if (ch == STX)
                ++recvCount;
            rec_ch = serial.read();
            continue;
        }

        idx = recvCount >> 1;

        // Am Ende den Empfang bestätigen und die erhaltene Antwort verarbeiten
        if (ch == ETX)
        {
            rm_send_ack();

            if (idx > 1)
                rm_process_msg(recvBuf, idx - 1); // Verarbeitung aufrufen (ohne Prüfsumme)

            recvCount = -1;

            return (true);
        }

        // Bei Überlauf die restlichen Zeichen ignorieren
        if (recvCount >= RECV_MAX_CHARS)
            return (true);

        // Die empfangenen Zeichen sind ein Hex String.
        // D.h. jeweils zwei Zeichen ergeben ein Byte.
        // In Answer gleich die dekodierten Bytes schreiben.
        //
        // Dieser Algorithmus ist fehlerhaft falls die Anzahl der empfangenen
        // Zeichen ungerade ist.

        if (ch >= '0' && ch <= '9')
            ch -= '0';
        else if (ch >= 'A' && ch <= 'F')
            ch -= 'A' - 10;
        else return (true); // Ungültige Zeichen ignorieren

        if (recvCount & 1)
        {
            recvBuf[idx] <<= 4;
            recvBuf[idx] |= ch;
        }
        else recvBuf[idx] = ch;

        ++recvCount;
        rec_ch = serial.read();
	} // while
	return (true);
}

bool rm_set_alarm_state(RmAlarmState newState)
{
    switch (newState)
    {
        case RM_ALARM :
            rm_send_hexstr((unsigned char*)"030210");
            break;

        case RM_TEST_ALARM :
            rm_send_hexstr((unsigned char*)"030280");
            break;

        case RM_NO_ALARM :
            rm_send_hexstr((unsigned char*)"030200");
            break;

        default : ;
    }
    return (true);
}

bool isReceiving()
{
    return (recvCount >= 0);
}
