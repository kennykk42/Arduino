/*
  WiFi.cpp - Library for Arduino Wifi shield.
  Copyright (c) 2011-2014 Arduino.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "ESP8266WiFi.h"
extern "C" {
#include "c_types.h"
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/dns.h"
}


extern "C" void esp_schedule();
extern "C" void esp_yield();


ESP8266WiFiClass::ESP8266WiFiClass()
{

}

void ESP8266WiFiClass::mode(WiFiMode m)
{
    ETS_UART_INTR_DISABLE();
    wifi_set_opmode(m);
    ETS_UART_INTR_ENABLE();
}

int ESP8266WiFiClass::begin(const char* ssid)
{
    return begin(ssid, 0);
}


int ESP8266WiFiClass::begin(const char* ssid, const char *passphrase)
{
    if (wifi_get_opmode() == WIFI_AP)
    {
        // turn on AP+STA mode
        mode(WIFI_AP_STA);
    }

    struct station_config conf;
    strcpy(reinterpret_cast<char*>(conf.ssid), ssid);
    if (passphrase)
        strcpy(reinterpret_cast<char*>(conf.password), passphrase);
    else
        *conf.password = 0;

    ETS_UART_INTR_DISABLE();
    wifi_station_set_config(&conf);
    wifi_station_connect();
    ETS_UART_INTR_ENABLE();
    wifi_station_dhcp_start();
    return 0;
}

void ESP8266WiFiClass::config(IPAddress local_ip, IPAddress gateway, IPAddress subnet)
{
    struct ip_info info;
    info.ip.addr = static_cast<uint32_t>(local_ip);
    info.gw.addr = static_cast<uint32_t>(gateway);
    info.netmask.addr = static_cast<uint32_t>(subnet);

    wifi_station_dhcpc_stop();
    wifi_set_ip_info(STATION_IF, &info);
}

int ESP8266WiFiClass::disconnect()
{
    struct station_config conf;
    *conf.ssid = 0;
    *conf.password = 0;
    ETS_UART_INTR_DISABLE();
    wifi_station_set_config(&conf);
    wifi_station_disconnect();
    ETS_UART_INTR_ENABLE();
    return 0;
}

void ESP8266WiFiClass::softAP(const char* ssid)
{
    softAP(ssid, 0);
}
  

void ESP8266WiFiClass::softAP(const char* ssid, const char* passphrase)
{
    if (wifi_get_opmode() == WIFI_STA)
    {
        // turn on AP+STA mode
        mode(WIFI_AP_STA);
    }

    struct softap_config conf;
    wifi_softap_get_config(&conf);
    strcpy(reinterpret_cast<char*>(conf.ssid), ssid);
    conf.channel = 1;

    if (!passphrase || strlen(passphrase) == 0)
    {
        conf.authmode = AUTH_OPEN;
        *conf.password = 0;
    }
    else
    {
        conf.authmode = AUTH_WPA2_PSK;
        strcpy(reinterpret_cast<char*>(conf.password), passphrase);
    }

    ETS_UART_INTR_DISABLE();
    wifi_softap_set_config(&conf);
    ETS_UART_INTR_ENABLE();
}


uint8_t* ESP8266WiFiClass::macAddress(uint8_t* mac)
{
    wifi_get_macaddr(STATION_IF, mac);
    return mac;
}

uint8_t* softAPmacAddress(uint8_t* mac)
{
    wifi_get_macaddr(SOFTAP_IF, mac);
    return mac;
}
   
IPAddress ESP8266WiFiClass::localIP()
{
    struct ip_info ip;
    wifi_get_ip_info(STATION_IF, &ip);
    return IPAddress(ip.ip.addr);
}

IPAddress softAPIP()
{
    struct ip_info ip;
    wifi_get_ip_info(SOFTAP_IF, &ip);
    return IPAddress(ip.ip.addr);    
}


IPAddress ESP8266WiFiClass::subnetMask()
{
    struct ip_info ip;
    wifi_get_ip_info(STATION_IF, &ip);
    return IPAddress(ip.netmask.addr);
}

IPAddress ESP8266WiFiClass::gatewayIP()
{
    struct ip_info ip;
    wifi_get_ip_info(STATION_IF, &ip);
    return IPAddress(ip.gw.addr);
}

char* ESP8266WiFiClass::SSID()
{
    static struct station_config conf;
    wifi_station_get_config(&conf);
    return reinterpret_cast<char*>(conf.ssid);
}

// uint8_t* ESP8266WiFiClass::BSSID(uint8_t* bssid)
// {
//  uint8_t* _bssid = WiFiDrv::getCurrentBSSID();
//  memcpy(bssid, _bssid, WL_MAC_ADDR_LENGTH);
//     return bssid;
// }

// int32_t ESP8266WiFiClass::RSSI()
// {
//     return WiFiDrv::getCurrentRSSI();
// }

// uint8_t ESP8266WiFiClass::encryptionType()
// {
//     return WiFiDrv::getCurrentEncryptionType();
// }

void ESP8266WiFiClass::_scanDone(void* result, int status)
{
    if (status != OK)
    {
        ESP8266WiFiClass::_scanCount = 0;
        ESP8266WiFiClass::_scanResult = 0;
    }
    else
    {
      ESP8266WiFiClass::_scanResult = result;
      int i = 0;
      for (bss_info* it = reinterpret_cast<bss_info*>(result); it; it = STAILQ_NEXT(it, next), ++i);
      ESP8266WiFiClass::_scanCount = i;
    }
    esp_schedule();   
}


int8_t ESP8266WiFiClass::scanNetworks()
{
    if (wifi_get_opmode() == WIFI_AP)
    {
        mode(WIFI_AP_STA);
    }
    int status = wifi_station_get_connect_status();
    if (status != STATION_GOT_IP && status != STATION_IDLE)
    {
        disconnect();
    }
    
    struct scan_config config; 
    config.ssid = 0;
    config.bssid = 0;
    config.channel = 0;
    wifi_station_scan(&config, reinterpret_cast<scan_done_cb_t>(&ESP8266WiFiClass::_scanDone));
    esp_yield();
    return ESP8266WiFiClass::_scanCount;
}

void * ESP8266WiFiClass::_getScanInfoByIndex(int i)
{
    if (!ESP8266WiFiClass::_scanResult || i > ESP8266WiFiClass::_scanCount)
        return 0;

    struct bss_info* it = reinterpret_cast<struct bss_info*>(ESP8266WiFiClass::_scanResult);
    for (; i; it = STAILQ_NEXT(it, next), --i);
    if (!it)
        return 0;

    return it;
}

char* ESP8266WiFiClass::SSID(uint8_t i)
{
    struct bss_info* it = reinterpret_cast<struct bss_info*>(_getScanInfoByIndex(i));
    if (!it)
        return 0;

    return reinterpret_cast<char*>(it->ssid);
}

int32_t ESP8266WiFiClass::RSSI(uint8_t i)
{
    struct bss_info* it = reinterpret_cast<struct bss_info*>(_getScanInfoByIndex(i));
    if (!it)
        return 0;

    return it->rssi;
}

uint8_t ESP8266WiFiClass::encryptionType(uint8_t i)
{
    struct bss_info* it = reinterpret_cast<struct bss_info*>(_getScanInfoByIndex(i));
    if (!it)
        return -1;

    int authmode = it->authmode;
    if (authmode == AUTH_OPEN)
        return ENC_TYPE_NONE;
    if (authmode == AUTH_WEP)
        return ENC_TYPE_WEP;
    if (authmode == AUTH_WPA_PSK || authmode == AUTH_WPA_WPA2_PSK)  // fixme: is this correct?
        return ENC_TYPE_TKIP;
    if (authmode == AUTH_WPA2_PSK)
        return ENC_TYPE_CCMP;
    return -1;
}

uint8_t ESP8266WiFiClass::status()
{
    int status = wifi_station_get_connect_status();

    if (status == STATION_GOT_IP)
      return WL_CONNECTED;
    else if (status == STATION_NO_AP_FOUND)
      return WL_NO_SSID_AVAIL;
    else if (status == STATION_CONNECT_FAIL || status == STATION_WRONG_PASSWORD)
      return WL_CONNECT_FAILED;
    else if (status == STATION_IDLE)
      return WL_IDLE_STATUS;
    else
      return WL_DISCONNECTED;
}

void wifi_dns_found_callback(const char *name, ip_addr_t *ipaddr, void *callback_arg)
{
    if (ipaddr)
        (*reinterpret_cast<IPAddress*>(callback_arg)) = ipaddr->addr;
    esp_schedule(); // resume the hostByName function
}

int ESP8266WiFiClass::hostByName(const char* aHostname, IPAddress& aResult)
{
    ip_addr_t addr;
    err_t err = dns_gethostbyname(aHostname, &addr, &wifi_dns_found_callback, &aResult);
    if (err == ERR_OK)
    {
        aResult = addr.addr;
    }
    else if (err == ERR_INPROGRESS)
    {
        esp_yield();
        // will return here when dns_found_callback fires
    }
    else // probably an invalid hostname
    {
        aResult = static_cast<uint32_t>(0);
    }
    return (aResult != 0) ? 1 : 0;
}

ESP8266WiFiClass WiFi;
