import Foundation

// MARK: - Epson ePOS SDK implementation
//
// Requires the Epson ePOS SDK to be added to the Xcode project:
//   1. Download from https://download.epson-biz.com/modules/pos/index.php?page=single_soft&cid=6836
//   2. Drag ePOS-Print_SDK_iOS/lib/libepos2.xcframework into the Xcode project
//   3. In Target → General → Frameworks, Libraries, and Embedded Content
//      set libepos2.xcframework to "Embed & Sign"
//
// Until the framework is linked the connection always returns failure,
// so the rest of the app continues to compile and run normally.
//
// Connection string format: "BT:<DeviceName or MAC>"
// e.g. "BT:EPSON_TM_T88" or "BT:00:01:02:03:04:05"

#if canImport(libepos2)
import libepos2

class EpsonPrinterConnection: PrinterConnection {

    private let printer: POSPrinter

    init(printer: POSPrinter) {
        self.printer = printer
    }

    func send(data: Data, completion: @escaping (Bool) -> Void) {
        guard let identifier = printer.bluetoothIdentifier, !identifier.isEmpty else {
            print("[Epson] No Bluetooth identifier configured for printer '\(printer.name)'")
            completion(false)
            return
        }

        let series = printer.epsonSeries.rawValue
        let target = "BT:\(identifier)"

        DispatchQueue.global(qos: .userInitiated).async {
            guard let eposPrinter = Epos2Printer(
                printerSeries: series,
                lang: EPOS2_MODEL_ANK.rawValue
            ) else {
                print("[Epson] Failed to create Epos2Printer for series \(series)")
                completion(false)
                return
            }

            let addResult = eposPrinter.addCommand(data)
            guard addResult == EPOS2_SUCCESS.rawValue else {
                print("[Epson] addCommand failed: \(addResult)")
                eposPrinter.clearCommandBuffer()
                completion(false)
                return
            }

            let connectResult = eposPrinter.connect(target, timeout: 15000)
            guard connectResult == EPOS2_SUCCESS.rawValue else {
                print("[Epson] connect('\(target)') failed: \(connectResult)")
                eposPrinter.clearCommandBuffer()
                completion(false)
                return
            }

            eposPrinter.beginTransaction()
            let sendResult = eposPrinter.sendData(Int32(EPOS2_PARAM_DEFAULT))
            eposPrinter.endTransaction()

            eposPrinter.disconnect()
            eposPrinter.clearCommandBuffer()

            let success = sendResult == EPOS2_SUCCESS.rawValue
            if !success { print("[Epson] sendData failed: \(sendResult)") }
            completion(success)
        }
    }
}

#else

// Stub used when the Epson SDK is not yet linked.
class EpsonPrinterConnection: PrinterConnection {

    private let printer: POSPrinter

    init(printer: POSPrinter) {
        self.printer = printer
    }

    func send(data: Data, completion: @escaping (Bool) -> Void) {
        print("[Epson] SDK not linked — add libepos2.xcframework to the Xcode project.")
        completion(false)
    }
}

#endif
