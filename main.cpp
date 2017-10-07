 /********************************************************************************
 *
 * V5 SYSTEMS
 * __________________
 *
 *  [2013] - [2017] V5 Systems Incorporated
 *  All Rights Reserved.
 *
 *  NOTICE:  This is free software.  Permission is granted to everyone to use,
 *          copy or modify this software under the terms and conditions of
 *                 GNU General Public License, v. 2
 *
 *
 *
 * GPL license.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA
 **********************************************************************************/

#include "stdio.h"
#include <iostream>
#include <string>
#include <openssl/rsa.h>

#include "wsdd.nsmap"

#include "include/soapDeviceBindingProxy.h"
#include "include/soapMediaBindingProxy.h"

#include "plugin/wsseapi.h"
#include "plugin/httpda.h"

#define ONVIF_WAIT_TIMEOUT 10

using namespace std;

char const* progName;
std::string tmpPass;
std::string tmpLogin;
std::vector<std::string> profNames;
std::vector<std::string> profTokens;
std::vector<std::string> profURIs;
int verbosity;

// Helper functions
bool sendGetWsdlUrl(DeviceBindingProxy* tProxyDevice, _tds__GetWsdlUrl * wsdURL,
                     _tds__GetWsdlUrlResponse * wsdURLResponse);
bool sendGetCapabilities(DeviceBindingProxy* tProxyDevice, _tds__GetCapabilities * getCap,
                     _tds__GetCapabilitiesResponse * getCapResponse, MediaBindingProxy * tProxyMedia);
bool sendGetProfiles(MediaBindingProxy* tProxyMedia, _trt__GetProfiles * getProfiles,
                     _trt__GetProfilesResponse * getProfilesResponse, soap * tSoap);
bool sendGetStreamUri(MediaBindingProxy* tProxyMedia, _trt__GetStreamUri * streamUri,
                     _trt__GetStreamUriResponse * streamUriResponse);

void printError(soap* _psoap){
	fprintf(stderr,"error:%d faultstring:%s faultcode:%s faultsubcode:%s faultdetail:%s\n",
        _psoap->error,	*soap_faultstring(_psoap), *soap_faultcode(_psoap),*soap_faultsubcode(_psoap),
        *soap_faultdetail(_psoap));
}

bool sendGetWsdlUrl(DeviceBindingProxy* tProxyDevice, _tds__GetWsdlUrl * wsdURL,
                     _tds__GetWsdlUrlResponse * wsdURLResponse){
  static int tCount=0;
  tCount++;
  if(tCount > 4) return false;
  int result = tProxyDevice->GetWsdlUrl(wsdURL, *wsdURLResponse);
	if (result == SOAP_OK){
    if(verbosity>2){
      fprintf(stderr, "WsdlUrl Found: %s \n", wsdURLResponse->WsdlUrl.c_str());
    }
		return true;
	}
	else{
	  if(verbosity>2)std::cout <<  "sendGetWsdlUrl return result: " << result << std::endl;
		if(verbosity>1)printError(tProxyDevice->soap);
    tProxyDevice->soap->userid = tmpLogin.c_str();
    tProxyDevice->soap->passwd = tmpPass.c_str();
    return sendGetWsdlUrl(tProxyDevice, wsdURL, wsdURLResponse);
	}
}

bool sendGetCapabilities(DeviceBindingProxy* tProxyDevice, _tds__GetCapabilities * getCap,
                     _tds__GetCapabilitiesResponse * getCapResponse, MediaBindingProxy * tProxyMedia){
  static int tCount=0;
  tCount++;
  if(tCount > 4) return false;
  int result = tProxyDevice->GetCapabilities(getCap, *getCapResponse);
	if (result == SOAP_OK){
		if (getCapResponse->Capabilities->Media != NULL)		{
			tProxyMedia->soap_endpoint = getCapResponse->Capabilities->Media->XAddr.c_str();
			if(verbosity>2)fprintf(stderr, "MediaUrl Found: %s\n",getCapResponse->Capabilities->Media->XAddr.c_str());
		}
		else{
      if(verbosity>1)std::cout <<  "sendGetCapabilities Media not found: "  << std::endl;
      return false;
		}
		return true;
	}
	else{
	  if(verbosity>2)std::cout <<  "sendGetCapabilities return result: " << result << std::endl;
		if(verbosity>1)printError(tProxyDevice->soap);
    tProxyDevice->soap->userid = tmpLogin.c_str();
    tProxyDevice->soap->passwd = tmpPass.c_str();
    return sendGetCapabilities(tProxyDevice, getCap, getCapResponse, tProxyMedia);
	}
}

bool sendGetProfiles(MediaBindingProxy* tProxyMedia, _trt__GetProfiles * getProfiles,
                     _trt__GetProfilesResponse * getProfilesResponse, soap * tSoap){
  static int tCount=0;
  tCount++;
  if(tCount > 4) return false;
  int result = tProxyMedia->GetProfiles(getProfiles, *getProfilesResponse);
	if (result == SOAP_OK){
		_trt__GetStreamUri *tmpGetStreamUri = soap_new__trt__GetStreamUri(tSoap, -1);
		tmpGetStreamUri->StreamSetup = soap_new_tt__StreamSetup(tSoap, -1);
		tmpGetStreamUri->StreamSetup->Stream = tt__StreamType__RTP_Unicast;
		tmpGetStreamUri->StreamSetup->Transport = soap_new_tt__Transport(tSoap, -1);
		tmpGetStreamUri->StreamSetup->Transport->Protocol = tt__TransportProtocol__RTSP;

		_trt__GetStreamUriResponse *tmpGetStreamUriResponse = soap_new__trt__GetStreamUriResponse(tSoap, -1);

		if(verbosity>2)fprintf(stderr, "*****MediaProfilesFound:\n");

		for (int i = 0; i < getProfilesResponse->Profiles.size(); i++){
			if(verbosity>2)fprintf(stderr, "\t%d Name:%s\n\t\tToken:%s\n", i, getProfilesResponse->Profiles[i]->Name.c_str(),
                           getProfilesResponse->Profiles[i]->token.c_str());
      profNames.push_back(getProfilesResponse->Profiles[i]->Name);
      profTokens.push_back(getProfilesResponse->Profiles[i]->token);
			tmpGetStreamUri->ProfileToken = getProfilesResponse->Profiles[i]->token;

			if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(tProxyMedia->soap, NULL, tmpLogin.c_str(),  tmpPass.c_str())){
				return false;
			}
			if (false == sendGetStreamUri(tProxyMedia, tmpGetStreamUri, tmpGetStreamUriResponse))	{
				return false;
			}
		}
		return true;
	}
	else{
	  if(verbosity>2)std::cout <<  "sendGetProfiles return result: " << result << std::endl;
		if(verbosity>1)printError(tProxyMedia->soap);
    tProxyMedia->soap->userid = tmpLogin.c_str();
    tProxyMedia->soap->passwd = tmpPass.c_str();
    return sendGetProfiles(tProxyMedia, getProfiles, getProfilesResponse, tSoap);
	}
}

bool sendGetStreamUri(MediaBindingProxy* tProxyMedia, _trt__GetStreamUri * streamUri,
                     _trt__GetStreamUriResponse * streamUriResponse){
  static int tCount=0;
  tCount++;
  if(tCount > 4) return false;
  int result = tProxyMedia->GetStreamUri(streamUri, *streamUriResponse);
	if (result == SOAP_OK){
    if(verbosity>2)fprintf(stderr, "\t\tRTSP:%s\n\n", streamUriResponse->MediaUri->Uri.c_str());
    profURIs.push_back(streamUriResponse->MediaUri->Uri);
		return true;
	}
	else{
	  if(verbosity>2)std::cout <<  "sendGetStreamUri return result: " << result << std::endl;
		if(verbosity>1)printError(tProxyMedia->soap);
    tProxyMedia->soap->userid = tmpLogin.c_str();
    tProxyMedia->soap->passwd = tmpPass.c_str();
    return sendGetStreamUri(tProxyMedia, streamUri, streamUriResponse);
	}
}

void usage() {
    std::cout << "Usage: " << progName << "\n"
        << "\t-l login \n"
        << "\t-i ip or host \n"
        << "\t-v output verbosity \n"
        << "\t-p password \n";
    exit(1);
}

int main(int argc, char **argv){
	DeviceBindingProxy proxyDevice;
	MediaBindingProxy proxyMedia;

	std::string outResults="{\"status\":\"error\"}";
	progName = argv[0];
  tmpPass="admin";
  tmpLogin="admin";
  verbosity=0;
	std::string tmpHost="127.0.0.1";

  for (int i=1; i<argc; i++){
      if(strcmp (argv[i],"-p") == 0){
        i++;
        tmpPass=std::string((const char*)argv[i]);
      }
      else if(strcmp (argv[i],"-v") == 0){
        i++;
        verbosity=atoi(argv[i]);
      }
      else if(strcmp (argv[i],"-i") == 0){
        i++;
        tmpHost=std::string((const char*)argv[i]);
      }
      else if(strcmp (argv[i],"-l") == 0){
        i++;
        tmpLogin=std::string((const char*)argv[i]);
      }
      else usage();
  }
  tmpHost="http://"+tmpHost+"/onvif/device_service";
  proxyDevice.soap_endpoint = tmpHost.c_str();

	struct soap *soap = soap_new();

	soap_register_plugin(proxyDevice.soap, soap_wsse);
	soap_register_plugin(proxyMedia.soap, soap_wsse);
	soap_register_plugin(proxyDevice.soap, http_da );
	soap_register_plugin(proxyMedia.soap, http_da );

  int result = SOAP_ERR;

	if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(proxyDevice.soap, NULL, tmpLogin.c_str(), tmpPass.c_str())){
    std::cout << outResults << std::endl;
		return -1;
	}

	if (SOAP_OK != soap_wsse_add_Timestamp(proxyDevice.soap, "Time", 10))	{
    std::cout << outResults << std::endl;
		return -2;
	}

	proxyDevice.soap->recv_timeout=ONVIF_WAIT_TIMEOUT;
	proxyDevice.soap->send_timeout=ONVIF_WAIT_TIMEOUT;
	proxyDevice.soap->connect_timeout=ONVIF_WAIT_TIMEOUT;

	_tds__GetWsdlUrl *tmpGetWsdlUrl = soap_new__tds__GetWsdlUrl(soap, -1);
	_tds__GetWsdlUrlResponse *tmpGetWsdlUrlResponse = soap_new__tds__GetWsdlUrlResponse(soap, -1);

	if(false == sendGetWsdlUrl(&proxyDevice, tmpGetWsdlUrl, tmpGetWsdlUrlResponse)){
    if(verbosity>2)std::cout <<  "sendGetWsdlUrl failed all attempts" << std::endl;
    std::cout << outResults << std::endl;
    return -10;
	}

    soap_destroy(soap);
    soap_end(soap);

	if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(proxyDevice.soap, NULL, tmpLogin.c_str(),  tmpPass.c_str()))	{
    std::cout << outResults << std::endl;
		return -1;
	}

	_tds__GetCapabilities *tmpGetCapabilities = soap_new__tds__GetCapabilities(soap, -1);
	tmpGetCapabilities->Category.push_back(tt__CapabilityCategory__All);
	_tds__GetCapabilitiesResponse *tmpGetCapabilitiesResponse = soap_new__tds__GetCapabilitiesResponse(soap, -1);

	if(false == sendGetCapabilities(&proxyDevice, tmpGetCapabilities, tmpGetCapabilitiesResponse, &proxyMedia)){
    if(verbosity>2)std::cout <<  "sendGetCapabilities failed all attempts" << std::endl;
    std::cout << outResults << std::endl;
    return -11;
	}

	soap_destroy(soap);
	soap_end(soap);

	if (SOAP_OK != soap_wsse_add_UsernameTokenDigest(proxyMedia.soap, NULL, tmpLogin.c_str(),  tmpPass.c_str())){
    std::cout << outResults << std::endl;
		return -1;
	}

	if (SOAP_OK != soap_wsse_add_Timestamp(proxyMedia.soap, "Time", 10))	{
    std::cout << outResults << std::endl;
		return -2;
	}

	proxyMedia.soap->recv_timeout=ONVIF_WAIT_TIMEOUT;
	proxyMedia.soap->send_timeout=ONVIF_WAIT_TIMEOUT;
	proxyMedia.soap->connect_timeout=ONVIF_WAIT_TIMEOUT;

	_trt__GetProfiles *tmpGetProfiles = soap_new__trt__GetProfiles(soap, -1);
	_trt__GetProfilesResponse *tmpGetProfilesResponse = soap_new__trt__GetProfilesResponse(soap, -1);

	if(false == sendGetProfiles(&proxyMedia, tmpGetProfiles, tmpGetProfilesResponse, soap)){
    if(verbosity>2)std::cout <<  "sendGetProfiles failed all attempts" << std::endl;
    std::cout << outResults << std::endl;
    return -12;
	}

	soap_destroy(soap);
	soap_end(soap);

	outResults="{\"status\":\"OK\"";
	if(profURIs.size()){
    outResults=outResults+", \"profiles\":[";
    for(unsigned i=0; i<profURIs.size(); i++){
      if(i!=0) outResults=outResults+", ";
      outResults=outResults+"{\"name\":\"" + profNames[i] + "\", ";
      outResults=outResults+"\"token\":\"" + profTokens[i] + "\", ";
      outResults=outResults+"\"uri\":\"" + profURIs[i] + "\"}";
    }
    outResults=outResults+"]";
	}
	outResults=outResults+"}";
  profNames.clear();
  profTokens.clear();
  profURIs.clear();

  std::cout << outResults << std::endl;
	return 0;
}

