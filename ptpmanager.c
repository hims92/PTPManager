#include "ptpmanager.h"
#include "stdlib.h"

#define PTP_GENERAL_PORT 4201
#define PTP_EVENT_PORT 4200


int out_sequence;
int in_sequence;
NetPath * netPath;

int packCommonHeader(Octet *message)
{
	MsgManagement *outgoing = (MsgManagement*)(message);
	
	outgoing->header.transportSpecific = 0x0;
	outgoing->header.messageType = MANAGEMENT;
    outgoing->header.versionPTP = 2;
	outgoing->header.domainNumber = 0;

    outgoing->header.flagField0 = 0x00;
    outgoing->header.flagField1 = 0x00;
    outgoing->header.correctionField.msb = 0;
    outgoing->header.correctionField.lsb = 0;

	memcpy(outgoing->header.sourcePortIdentity.clockIdentity, \
			"00:26:9e:ff:fe:a65b:7e", CLOCK_IDENTITY_LENGTH);	
	outgoing->header.sourcePortIdentity.portNumber = 1;
	outgoing->header.sequenceId = in_sequence+1;
	outgoing->header.controlField = 0x0; /* deprecrated for ptp version 2 */
	outgoing->header.logMessageInterval = 0x7F;

	out_length += HEADER_LENGTH;
}


int packManagementHeader(Octet *message)
{
	int offset = sizeof(MsgHeader);
	MsgManagement *manage = (MsgManagement*)(message);
	manage->targetPortIdentity.portNumber = 4;	
	/* init managementTLV */
	manage->tlv = (ManagementTLV*)malloc(sizeof(ManagementTLV));
	manage->tlv->dataField = NULL;
	out_length += MANAGEMENT_LENGTH;
	
}

Boolean 
netInit(char *ifaceName)
{
	int temp;
	struct in_addr interfaceAddr, netAddr;
	struct sockaddr_in addr;

	
	printf("netInit\n");

	/* open sockets */
	if ((netPath->eventSock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0
	    || (netPath->generalSock = socket(PF_INET, SOCK_DGRAM, 
					      IPPROTO_UDP)) < 0) {
		printf("failed to initalize sockets");
		return FALSE;
	}
	
	temp = 1;			/* allow address reuse */
	if (setsockopt(netPath->eventSock, SOL_SOCKET, SO_REUSEADDR, 
		       &temp, sizeof(int)) < 0
	    || setsockopt(netPath->generalSock, SOL_SOCKET, SO_REUSEADDR, 
			  &temp, sizeof(int)) < 0) {
		printf("failed to set socket reuse\n");
	}


	/* bind sockets */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(PTP_GENERAL_PORT);

/*	if (bind(netPath->generalSock, (struct sockaddr *)&addr, 
		 sizeof(struct sockaddr_in)) < 0) {
		printf("failed to bind general socket");
		return FALSE;
	}

	addr.sin_port = htons(PTP_EVENT_PORT);
	if (bind(netPath->eventSock, (struct sockaddr *)&addr, 
		 sizeof(struct sockaddr_in)) < 0) {
		printf("failed to bind event socket");
		return FALSE;
	}
*/

#ifdef linux
	/*
	 * The following code makes sure that the data is only received on the specified interface.
	 * Without this option, it's possible to receive PTP from another interface, and confuse the protocol.
	 * Calling bind() with the IP address of the device instead of INADDR_ANY does not work.
	 *
	 * More info:
	 *   http://developerweb.net/viewtopic.php?id=6471
	 *   http://stackoverflow.com/questions/1207746/problems-with-so-bindtodevice-linux-socket-option
	 */
	if (setsockopt(netPath->eventSock, SOL_SOCKET, SO_BINDTODEVICE,
			ifaceName, strlen(ifaceName)) < 0
		|| setsockopt(netPath->generalSock, SOL_SOCKET, SO_BINDTODEVICE,
			ifaceName, strlen(ifaceName)) < 0){
			
		printf("failed to call SO_BINDTODEVICE on the interface");
		return FALSE;
		
	}

#endif
	return TRUE;
}


ssize_t 
netSendGeneral(Octet * buf, UInteger16 length, char *ip)
{
	ssize_t ret;
	struct sockaddr_in addr;
	int i;
	int x = 10;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PTP_GENERAL_PORT);
	if( (inet_pton(PF_INET, ip, &addr.sin_addr) ) == 0)
	{
		printf("\nAddress is not parsable\n");
		return 0;
	}
	else if ( (inet_pton(AF_INET, ip, &addr.sin_addr) ) < 0)
	{
		printf("\nAddress family is not supproted by this function\n");
		return 0;
	}
	
	ret = sendto(netPath->generalSock, buf, out_length, 0, \
			(struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (ret <= 0)
	printf("error sending uni-cast general message\n");

	return ret;
}



int main(int argc, char *argv[ ])
{

	Octet *outmessage = (Octet*)(malloc(PACKET_SIZE));
	memset(outmessage,0,sizeof(MANAGEMENT_LENGTH));
	netPath = (NetPath*)malloc(sizeof(NetPath));
	unsigned short tlvtype;
	out_sequence = 0;
	in_sequence = 0;
	out_length = 0;
	
	if(argc !=3)
	{
		printf("\nThe input is not in the required format. Please give the IP of PTP daemon\n");
		exit(0);
	}

	if (!netInit(argv[2])) {
		printf("failed to initialize network\n");
		return;
	}
	
	
	printf("Packing Header..\n");
	packCommonHeader(outmessage);	
	
	printf("Packing Management Header...\n");		
	packManagementHeader(outmessage);
	
	printf("Sending message....\n");
	
	if (!netSendGeneral(outmessage, sizeof(outmessage), argv[1]))
	{
		printf("Error sending message\n");
	}

	
}


