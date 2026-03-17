import Foundation

class PrinterService {

    private func getConnection(for printer: POSPrinter) -> PrinterConnection? {
        switch printer.connectionType {
        case .network:
            return NetworkPrinterConnection(printer: printer)
        case .bluetooth:
            return NetworkPrinterConnection(printer: printer)
        case .epsonBluetooth:
            return EpsonPrinterConnection(printer: printer)
        }
    }

    func sendTestPrint(to printer: POSPrinter, completion: @escaping (Bool, String) -> Void) {
        guard let connection = getConnection(for: printer) else {
            completion(false, "Invalid printer configuration.")
            return
        }

        let testMessage = "Hello, Printer! This is a test print.\n\n\n"
        let data = Data(testMessage.utf8)

        // ESC/POS command to initialize printer
        let initializeCommand = Data([0x1B, 0x40])
        // ESC/POS command to cut paper
        let cutCommand = Data([0x1D, 0x56, 0x41, 0x00])

        var fullCommand = Data()
        fullCommand.append(initializeCommand)
        fullCommand.append(data)
        fullCommand.append(cutCommand)

        connection.send(data: fullCommand) { success in
            DispatchQueue.main.async {
                if success {
                    completion(true, "Test print sent successfully!")
                } else {
                    completion(false, "Failed to send test print.")
                }
            }
        }
    }
}
