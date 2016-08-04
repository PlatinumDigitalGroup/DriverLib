#include <string>
#include <stdio.h>

#include <windows.h>

namespace DriverLib {

	typedef struct _LSA_UNICODE_STRING {
		USHORT Length;
		USHORT MaximumLength;
		PWSTR  Buffer;
	} LSA_UNICODE_STRING, *PLSA_UNICODE_STRING, UNICODE_STRING, *PUNICODE_STRING;

	NTSTATUS RtlAdjustPrivilege(DWORD privilege, bool enable, bool threadOnly, bool* previous) {
		typedef NTSTATUS(__stdcall *RtlAdjustPrivilege_t)(DWORD, bool, bool, bool*);
		HINSTANCE ntdll = LoadLibraryW(L"ntdll.dll");
		RtlAdjustPrivilege_t func = reinterpret_cast<RtlAdjustPrivilege_t>(GetProcAddress(ntdll, "RtlAdjustPrivilege"));
		return func(privilege, enable, threadOnly, previous);
	}

	inline NTSTATUS NtLoadDriver(PUNICODE_STRING driverPath) {
		typedef NTSTATUS(__stdcall *NtLoadDriver_t)(PUNICODE_STRING);
		HINSTANCE ntdll = LoadLibraryW(L"ntdll.dll");
		NtLoadDriver_t func = reinterpret_cast<NtLoadDriver_t>(GetProcAddress(ntdll, "NtLoadDriver"));
		return func(driverPath);
	}

	inline NTSTATUS NtUnloadDriver(PUNICODE_STRING driverPath) {
		typedef NTSTATUS(__stdcall *NtUnloadDriver_t)(PUNICODE_STRING);
		HINSTANCE ntdll = LoadLibraryW(L"ntdll.dll");
		NtUnloadDriver_t func = reinterpret_cast<NtUnloadDriver_t>(GetProcAddress(ntdll, "NtUnloadDriver"));
		return func(driverPath);
	}

	inline void RtlInitUnicodeString(PUNICODE_STRING dest, PCWSTR source) {
		typedef void(__stdcall *RtlInitUnicodeString_t)(PUNICODE_STRING, PCWSTR);
		HINSTANCE ntdll = LoadLibraryW(L"ntdll.dll");
		RtlInitUnicodeString_t func = reinterpret_cast<RtlInitUnicodeString_t>(GetProcAddress(ntdll, "RtlInitUnicodeString"));
		func(dest, source);
	}

	namespace Utils {
		
		std::wstring toRegistryFilePath(std::wstring original) {
			if (original.compare(0, 4, L"\\??\\") == 0) {
				return original;
			} else {
				return L"\\??\\" + original;
			}
		}

		std::wstring toNTRegistryPath(std::wstring serviceName) {
			return L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" + serviceName;
		}

		std::wstring toDeviceFile(std::wstring deviceName) {
			return L"\\.\\" + deviceName;
		}

		NTSTATUS addDriverPrivilege() {
			return RtlAdjustPrivilege(10, true, false, nullptr);
		}

		NTSTATUS loadDriver(std::wstring driverPath) {
			UNICODE_STRING unicodeDriverPath;
			RtlInitUnicodeString(&unicodeDriverPath, driverPath.c_str());
			return NtLoadDriver(&unicodeDriverPath);
		}

		NTSTATUS unloadDriver(std::wstring driverPath) {
			UNICODE_STRING unicodeDriverPath;
			RtlInitUnicodeString(&unicodeDriverPath, driverPath.c_str());
			return NtUnloadDriver(&unicodeDriverPath);
		}

		NTSTATUS createRegistryEntry(std::wstring serviceName, std::wstring imagePath) {
			NTSTATUS status = 0;
			std::wstring ntPath = Utils::toRegistryFilePath(imagePath);

			HKEY servicesKey;
			status = RegOpenKeyW(HKEY_LOCAL_MACHINE, L"system\\CurrentControlSet\\Services", &servicesKey);

			HKEY serviceKey;
			status = RegCreateKeyW(servicesKey, serviceName.c_str(), &serviceKey);

			if (status) return status;

			DWORD type = 1;
			status = RegSetValueExW(serviceKey, L"Type", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&type), sizeof(DWORD));
			status = RegSetValueExW(serviceKey, L"ImagePath", 0, REG_SZ, reinterpret_cast<const BYTE*>(ntPath.c_str()), ntPath.size() * sizeof(wchar_t) + 1);

			return status;
		}

		NTSTATUS deleteRegistryEntry(std::wstring serviceName) {
			return SHDeleteKeyW(HKEY_LOCAL_MACHINE, (L"SYSTEM\\CurrentControlSet\\Services\\" + serviceName).c_str());
		}

	}

	
	class IDriver {
	public:
		
		/**
		 * Driver destructor.
		 */
		virtual ~IDriver() {}

		/**
		 * Gets the unique service name for this driver.
		 */ 
		virtual std::wstring getServiceName() = 0;

		/**
		 * Loads the driver instance into kernel space.
		 */
		virtual NTSTATUS load() = 0;

		/**
		* Unloads the driver (if running) from the system.
		*/
		virtual NTSTATUS unload() = 0;

		/**
		* Returns whether or not the driver is currently loaded by the system.
		*/
		virtual bool isLoaded() = 0;

		/**
		* Sends an IRP IOCTL request to the driver.
		*/
		virtual NTSTATUS sendIOControlRequest(DWORD controlCode, void* inBuffer, size_t inBufferSize) = 0;


		/**
		* Sends an IRP IOCTL request to the driver and stores the response into outBuffer.
		*/
		virtual NTSTATUS sendIOControlRequest(DWORD controlCode, void* inBuffer, size_t inBufferSize, void* outBuffer, size_t outBufferSize) = 0;

		/**
		* Bridge method to tidy IO request calls.
		* I is the input buffer struct type
		* O is the output buffer struct type		*/
		template<typename I, typename O> NTSTATUS sendIOControlRequest(DWORD controlCode, I* inBuffer, O* outBuffer) {
			this->sendIOControlRequest(controlCode, inBuffer, sizeof(I), outBuffer, sizeof(O));
		}

		/**
		* Bridge method to tidy IO request calls.
		* I is the input buffer struct type
		*/
		template<typename I> NTSTATUS sendIOControlRequest(DWORD controlCode, I* inBuffer) {
			this->sendIOControlRequest(controlCode, inBuffer, sizeof(I));
		}

	};

	class GenericDriver : public IDriver {
	private:
		std::wstring filePath;
		std::wstring serviceName;
		std::wstring deviceName;

		HANDLE deviceHandle;

		bool loaded = false;
	public:
		GenericDriver(std::wstring filePath, std::wstring serviceName, std::wstring deviceName): deviceHandle(nullptr) {
			this->filePath = Utils::toRegistryFilePath(filePath);
			this->serviceName = serviceName;
			this->deviceName = deviceName;
		}

		std::wstring getServiceName() override {
			return serviceName;
		}

		NTSTATUS sendIOControlRequest(DWORD controlCode, void* inBuffer, size_t inBufferSize) override {
			return this->sendIOControlRequest(controlCode, inBuffer, inBufferSize, NULL, NULL);
		}

		NTSTATUS sendIOControlRequest(DWORD controlCode, void* inBuffer, size_t inBufferSize, void* outBuffer, size_t outBufferSize) override {
			if (DeviceIoControl(acquireDeviceHandle(), controlCode, inBuffer, inBufferSize, outBuffer, outBufferSize, NULL, NULL)) {
				return 0;
			} else {
				return GetLastError();
			}
		}

		NTSTATUS load() override {
			NTSTATUS status = Utils::addDriverPrivilege();

			if (status) {
				return status;
			}

			Utils::createRegistryEntry(serviceName, filePath);
			status = Utils::loadDriver(Utils::toNTRegistryPath(this->serviceName));

			if (!status) {
				loaded = true;
			}

			return status;
		}

		NTSTATUS unload() override {
			NTSTATUS status = Utils::addDriverPrivilege();

			if (status) {
				return status;
			}

			status = Utils::unloadDriver(Utils::toNTRegistryPath(this->serviceName));
			Utils::deleteRegistryEntry(serviceName);

			if (!status) {
				loaded = false;
			}

			return status;
		}

		bool isLoaded() override {
			// todo: better loaded check
			return loaded;
		}


		HANDLE acquireDeviceHandle() {
			if (!deviceHandle) {
				std::wstring deviceFile = Utils::toDeviceFile(deviceName);
				this->deviceHandle = CreateFileW(deviceFile.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
			}
			return deviceHandle;
		}
	};

	class BufferedDriver : public GenericDriver {
	private:
		BYTE* fileBuffer;
		size_t fileBufferSize;
		std::wstring tempFilePath;

	public:

		BufferedDriver(BYTE* fileBuffer, size_t fileBufferSize, std::wstring tempFilePath, std::wstring serviceName, std::wstring deviceName) : GenericDriver(tempFilePath, serviceName, deviceName) {
			this->fileBuffer = fileBuffer;
			this->fileBufferSize = fileBufferSize;
			this->tempFilePath = tempFilePath;
		}

		void writeBufferToTempFile() {
			std::ofstream ofile(tempFilePath, std::ios::binary);
			ofile.write(reinterpret_cast<char*>(fileBuffer), fileBufferSize);
			ofile.close();
		}

		NTSTATUS load() override {
			writeBufferToTempFile();
			return GenericDriver::load();
		}

		NTSTATUS unload() override {
			NTSTATUS status = GenericDriver::unload();
			if (!status) {
				DeleteFileW(tempFilePath.c_str());
			}
			return status;
		}

	};

	class DriverFactory {
	private:
		std::wstring filePath;
		std::wstring serviceName;
		std::wstring deviceName;
		
		// buffered
		std::wstring tempFilePath;
		BYTE* fileBuffer;
		size_t fileBufferSize;
		
	public:

		DriverFactory(std::wstring serviceName): fileBuffer(nullptr), fileBufferSize(0) {
			this->serviceName = serviceName;
		}

		IDriver* build() {
			IDriver* result;

			if (filePath.size() > 0) {
				// Driver is a GenericDriver because we have a filesystem path
				// if deviceName is not set, we need one so pass in so we use serviceName again
				result = new GenericDriver(filePath, serviceName, (deviceName.size() > 0) ? deviceName : serviceName);
			} else if (fileBuffer != nullptr) {
				// Driver is a BufferedDriver because we have a buffer and not a path
				result = new BufferedDriver(fileBuffer, fileBufferSize, tempFilePath, serviceName, (deviceName.size() > 0) ? deviceName : serviceName);
			} else {
				// Driver has no file buffer and has no file path, so we have nothing to load
				return nullptr;
			}

			return result;
		}

		DriverFactory* setDeviceName(std::wstring device) {
			this->deviceName = device;
			return this;
		}

		DriverFactory* setFilePath(std::wstring path) {
			this->filePath = path;
			return this;
		}

		DriverFactory* setFileBuffer(BYTE* buffer, size_t size) {
			this->fileBuffer = buffer;
			this->fileBufferSize = size;
			return this;
		}

		DriverFactory* setTempFilePath(std::wstring path) {
			this->tempFilePath = path;
			return this;
		}

	};

}
