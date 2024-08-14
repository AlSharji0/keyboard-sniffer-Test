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

NTSTATUS keyboardDispatchRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(
		Irp,
		keyboardReadComplete,
		NULL,
		TRUE,
		TRUE,
		TRUE
	);

	return IoCallDriver(g_KeyboardDevice, Irp);
}

VOID DriverUnload(IN PDRIVER_OBJECT DriverObject) {
	if (g_MyDeviceObject) {
		IoDeleteDevice(g_MyDeviceObject);
	}

	if (g_KeyboardDevice) {
		IoDetachDevice(g_KeyboardDevice);
	}

	DbgPrint("Driver Unloaded\n");
}


NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING RegistryPath) {
	UNICODE_STRING deviceName;
	UNICODE_STRING dosDeviceName;

	NTSTATUS status = IoCreateDevice(
		DriverObject,
		0,
		&deviceName,
		FILE_DEVICE_KEYBOARD,
		FILE_DEVICE_KEYBOARD,
		FALSE,
		&g_MyDeviceObject
	);

	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to create device\n");
		return status;
	}
	DriverObject->DriverUnload = DriverUnload;
	DriverObject->MajorFunction[IRP_MJ_READ] = keyboardDispatchRoutine;

	UNICODE_STRING keyboardDeviceName;
	RtlInitUnicodeString(&keyboardDeviceName, L"\\Device\\KeyboardClass0");

	status = IoAttachDevice(g_MyDeviceObject, &keyboardDeviceName, &g_KeyboardDevice);

	if (!NT_SUCCESS(status)) {
		IoDeleteDevice(g_MyDeviceObject);
		DbgPrint("Failed to attach to keyboard device\n");
		return status;
	}

	DbgPrint("Driver Loaded Successfully\n");
	return STATUS_SUCCESS;
}
}
