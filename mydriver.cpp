#include <ntddk.h>
#include <kbdmou.h>

#define DEVICE_NAME L"\\Device\\MyKeyboardFilter"
#define DOS_DEVICE_NAME L"\\DosDevices\\MyKeyboardFilter"

PDEVICE_OBJECT g_KeyboardDevice = NULL;
PDEVICE_OBJECT g_MyDeviceObject = NULL;

extern "C" {

// Function too handle keyboard read IRPs.
NTSTATUS keyboardReadComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context) {
	if (NT_SUCCESS(Irp->IoStatus.Status)) {
		PKEYBOARD_INPUT_DATA keys = (PKEYBOARD_INPUT_DATA)Irp->AssociatedIrp.SystemBuffer;
		ULONG keycount = Irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);

		for (ULONG i = 0; i < keycount; i++) {
			DbgPrint("Key Pressed: 0x%x\n", keys[i].MakeCode);
		}
	}

	return Irp->IoStatus.Status;
}
}