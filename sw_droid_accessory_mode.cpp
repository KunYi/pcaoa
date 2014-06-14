#include <cassert>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <libusb-1.0/libusb.h> 
#include <alsa/asoundlib.h>

/* Samsung N7100 04e8:6866 */
#define VID_TEST (0x04e8)
#define PID_TEST (0x6866)
/* Xaomi hougmi */
/* not support accessory mode
#define VID_TEST (0x2717)
#define PID_TEST (0x1220)
*/

/* 
 * AOA 2.0 protocol
 * ref. https://source.android.com/accessories/aoa2.html 
 */
// LIBUSB_ENDPOINT_IN == DEVICE_TO_HOST
// LIBUSB_ENDPOINT_OUT == HOST_TO_DEVICE
#define AOA_CTRL_OUT (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT)
#define AOA_CTRL_IN (LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN)

#define AOA_REQ_PROTOCOL				(51)
#define AOA_REQ_SETPROTO				(52)
#  define AOA_PROTO_MANUFACTURE_INDEX			(0)
#  define AOA_PROTO_MODEL_INDEX 			(1)
#  define AOA_PROTO_DESCRIPTION_INDEX			(2)
#  define AOA_PROTO_VERSION_INDEX			(3)
#  define AOA_PROTO_URI_INDEX				(4)
#  define AOA_PROTO_SERIAL_INDEX			(5)
#define AOA_REQ_ACCESSORY				(53)
#define AOA_REQ_REGISTER_HID				(54)
#define AOA_REQ_UNREGISTER_HID				(55)
#define AOA_REQ_SET_HID_REPORT				(56)
#define AOA_SEND_HID_EVENT				(57)
#define AOA_REQ_AUDIO					(58)
#define VID_GOOGLE					(0x18d1)
#define AOA_PID_BASE					(0x2d00) /* accessory */
#define AOA_PID_WITH_ADB				(0x2d01) /* accessory + adb */
#define AOA_PID_AUDIO_ONLY				(0x2d02) /* audio */
#define AOA_PID_AUDIO_WITH_ADB				(0x2d03) /* audio + adb */
#define AOA_PID_WITH_AUDIO				(0x2d04) /* accessory + audio */
#define AOA_PIO_WITH_AUDIO_ADB				(0x2d05) /* accessory + audio + adb */

/*
 * from adk2012/board/library/usbh.h
 */
#define ADK2012_MANUFACTURE_STRING			("Google, Inc.")
#define ADK2012_MODE_STRING				("DemoKit")
#define ADK2012_DESCRIPTION_STRING			("DemoKit Arduino Board")
#define ADK2012_VERSION_STRING				("2.0")
#define ADK2012_URI_STRING				("http://www.android.com")
#define ADK2012_SERIAL_STRING				("0000000012345678")

static libusb_context *context = NULL;
static libusb_device **list = NULL;
static libusb_device *found = NULL;
static libusb_device_descriptor desc_dev = {0};

/*
 * Audio only support 2 channel, 16-bit PCM audio format 
 * to usage standard USB audio class
 *
 */

int getProtocl(libusb_device_handle *dev, uint16_t* protocol) {
	return libusb_control_transfer(dev,
		AOA_CTRL_IN | LIBUSB_RECIPIENT_DEVICE, // bmRequestType
		AOA_REQ_PROTOCOL, // bRequest
		0, // value
		0, // index
		(uint8_t*) protocol, // data buffer
		2,    // 2 byte
		500); // timeout 500ms
}

int setProto(libusb_device_handle *dev, int idx,const char* str)
{
	return libusb_control_transfer(dev,
			AOA_CTRL_OUT | LIBUSB_RECIPIENT_DEVICE,
			AOA_REQ_SETPROTO,
			0,
			idx,
			(unsigned char*)str,
			strlen(str) + 1,
			500); // timeout
}

int switchToAccessoryMode(libusb_device_handle *dev)
{
	return libusb_control_transfer(dev,
			AOA_CTRL_OUT | LIBUSB_RECIPIENT_DEVICE,
			AOA_REQ_ACCESSORY,
			0,
			0,
			NULL,
			0,
			500);
}

int setAudioMode(libusb_device_handle *dev, bool mode)
{
	int value = mode ? 1 : 0;
	return libusb_control_transfer(dev,
			AOA_CTRL_OUT | LIBUSB_RECIPIENT_DEVICE,
			AOA_REQ_AUDIO,
			value,
			0,
			NULL,
			0,
			500);
}

bool isInteresting(libusb_device* device, uint16_t vid, uint16_t pid);
bool isAccessoryDevice(libusb_device_descriptor  *desc);
void prt_dev_desc(libusb_device_descriptor *desc);


int main() {
	int rc = 0;
	size_t count = 0;

	rc = libusb_init(&context);
	assert(rc == 0);
	
	count = libusb_get_device_list(context, &list);
	assert(count > 0);
	for (size_t idx = 0; idx < count; ++idx) {
		libusb_device *device = list[idx];
		libusb_device_descriptor desc = {0};
		rc = libusb_get_device_descriptor(device, &desc);
		assert(rc == 0);
		if (isInteresting(device, VID_TEST, PID_TEST)) {
			found = device;
		}
	}

// to enable accessory mode
	if(found) {
		libusb_device_descriptor desc = {0};
		rc = libusb_get_device_descriptor(found, &desc);
		assert(rc == 0);
		printf("found a device, %04x:%04x\n", desc.idVendor, desc.idProduct);

		libusb_device_handle* handle = NULL;

		int err = 0;
		err = libusb_open(found, &handle);
		if (err < 0) {
			printf("open device failed\n");
			goto error;
		}

		uint16_t protocol;
		err = getProtocl(handle, &protocol);
		if (err < 0) {
			printf("it is not a ndroid-powerd device\n");
			goto error;
		}

#if 0
		setProto(handle, AOA_PROTO_MANUFACTURE_INDEX, "KunYi Chen");
		setProto(handle, AOA_PROTO_MODEL_INDEX, "PC Host");
		setProto(handle, AOA_PROTO_DESCRIPTION_INDEX, "PC Host to emulation an android accessory");
		setProto(handle, AOA_PROTO_VERSION_INDEX, "0.1");
		setProto(handle, AOA_PROTO_URI_INDEX, "kunyichen.wordpress.com");
		setProto(handle, AOA_PROTO_SERIAL_INDEX, "12345678-001");
#else
		setProto(handle, AOA_PROTO_MANUFACTURE_INDEX, ADK2012_MANUFACTURE_STRING);
		setProto(handle, AOA_PROTO_MODEL_INDEX, ADK2012_MODE_STRING);
		setProto(handle, AOA_PROTO_DESCRIPTION_INDEX, ADK2012_DESCRIPTION_STRING);
		setProto(handle, AOA_PROTO_VERSION_INDEX, ADK2012_VERSION_STRING);
		setProto(handle, AOA_PROTO_URI_INDEX, ADK2012_URI_STRING);
		setProto(handle, AOA_PROTO_SERIAL_INDEX, ADK2012_SERIAL_STRING);
#endif

		if (protocol == 2) {
			setAudioMode(handle, true);
		}
		switchToAccessoryMode(handle);

error:
		if (handle)
			libusb_close(handle);
	}
	
	libusb_free_device_list(list,1);

	sleep(2); // for device re-attached again
	//
	// enumeration USB device again, to get droid accessory mode device
	//
	count = libusb_get_device_list(context, &list);
	assert(count > 0);

	found = NULL;
	for (size_t idx = 0; idx < count; ++idx) {
	        libusb_device *device = list[idx];
	        libusb_device_descriptor desc = {0};
	        rc = libusb_get_device_descriptor(device, &desc);
	        assert(rc == 0);
		if (isAccessoryDevice(&desc)) {
			found = device;
			break;
		}
	}

	if (!found) {
		printf("success switch to accessory mode\n");
		goto final;
	}

	//
	//  read device configuration
	//
	libusb_get_device_descriptor(found, &desc_dev);
	prt_dev_desc(&desc_dev);


final:
	libusb_free_device_list(list,1);
	libusb_exit(context);
}

bool isInteresting(libusb_device* device, uint16_t vid, uint16_t pid) {
	libusb_device_descriptor desc = { 0 };
	libusb_get_device_descriptor(device, & desc);
return ((desc.idVendor == vid) && (desc.idProduct == pid)) ? true : false;
}

bool isAccessoryDevice(libusb_device_descriptor* desc)
{
	return ((desc->idVendor == VID_GOOGLE) && 
		(desc->idProduct >= 0x2d00) &&
		(desc->idProduct <= 0x2d05)) ? true : false;
}

void prt_dev_desc(libusb_device_descriptor *desc)
{
	printf("\n\n");
	printf("dump device description\n");
	printf("Accessory(%04x:%04x)\n", desc->idVendor, desc->idProduct);
	printf("DeviceClass:0x%02x, Subclass:0x%02x\n", desc->bDeviceClass, desc->bDeviceSubClass);
	printf("Device Protocol:0x%02x, MaxPacketSize of ep0:%4d\n", desc->bDeviceProtocol, desc->bMaxPacketSize0);
	printf("Num of Configurations:%d\n", desc->bNumConfigurations);
	printf("\n\n");
}
