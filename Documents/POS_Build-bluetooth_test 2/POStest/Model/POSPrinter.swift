import Foundation

enum PrinterConnectionType: String, Codable, CaseIterable {
    case network = "Network"
    case bluetooth = "Bluetooth LE"
    case epsonBluetooth = "Epson (Bluetooth)"
}

struct POSPrinter: Identifiable, Codable, Hashable {
    var id = UUID()
    var name: String
    var connectionType: PrinterConnectionType

    // Network properties
    var ipAddress: String?
    var port: Int?

    // Bluetooth properties
    var bluetoothIdentifier: String?
}
