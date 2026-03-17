import Foundation

enum PrinterConnectionType: String, Codable, CaseIterable {
    case network = "Network"
    case bluetooth = "Bluetooth LE"
    case epsonBluetooth = "Epson (Bluetooth)"
}

/// Epson printer series — maps directly to Epos2PrinterSeries enum values.
enum EpsonPrinterSeries: Int, Codable, CaseIterable, Identifiable {
    case tmT88  = 1
    case tmT20  = 7
    case tmT82  = 15
    case tmM10  = 22
    case tmM30  = 23

    var id: Int { rawValue }

    var displayName: String {
        switch self {
        case .tmT88: return "TM-T88"
        case .tmT20: return "TM-T20"
        case .tmT82: return "TM-T82"
        case .tmM10: return "TM-m10"
        case .tmM30: return "TM-m30"
        }
    }
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

    // Epson-specific: printer series (default TM-T88)
    var epsonSeries: EpsonPrinterSeries = .tmT88
}
