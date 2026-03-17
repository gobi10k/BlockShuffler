import Foundation
import libepos2

// MARK: - Epson ePOS SDK implementation
//
// Uses Epos2Printer to connect over Bluetooth and send raw ESC/POS data
// via addCommand(). The printer series is stored on POSPrinter so the
// correct Epos2PrinterSeries constant is used for each device.
//
// Connection string format for Bluetooth: "BT:<DeviceName or MAC>"
// e.g. "BT:EPSON_TM_T88" or "BT:00:01:02:03:04:05"
//
// All SDK calls are performed on a background thread. The completion
// callback is dispatched to the main thread by the caller (PrinterService).

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
            // Create printer object for the configured series.
            // lang: EPOS2_MODEL_ANK (0) covers most Latin-alphabet receipt printers.
            guard let eposPrinter = Epos2Printer(
                printerSeries: series,
                lang: EPOS2_MODEL_ANK.rawValue
            ) else {
                print("[Epson] Failed to create Epos2Printer for series \(series)")
                completion(false)
                return
            }

            // Load raw ESC/POS bytes as a command buffer entry.
            let addResult = eposPrinter.addCommand(data)
            guard addResult == EPOS2_SUCCESS.rawValue else {
                print("[Epson] addCommand failed: \(addResult)")
                eposPrinter.clearCommandBuffer()
                completion(false)
                return
            }

            // Connect (blocking, 15-second timeout).
            let connectResult = eposPrinter.connect(target, timeout: 15000)
            guard connectResult == EPOS2_SUCCESS.rawValue else {
                print("[Epson] connect('\(target)') failed: \(connectResult)")
                eposPrinter.clearCommandBuffer()
                completion(false)
                return
            }

            // Send the buffered command.
            eposPrinter.beginTransaction()
            let sendResult = eposPrinter.sendData(Int32(EPOS2_PARAM_DEFAULT))
            eposPrinter.endTransaction()

            // Disconnect and clean up regardless of send result.
            eposPrinter.disconnect()
            eposPrinter.clearCommandBuffer()

            let success = sendResult == EPOS2_SUCCESS.rawValue
            if !success {
                print("[Epson] sendData failed: \(sendResult)")
            }
            completion(success)
        }
    }
}
