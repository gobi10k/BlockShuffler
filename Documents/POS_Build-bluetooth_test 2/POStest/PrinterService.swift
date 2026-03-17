import Foundation

// MARK: - ESC/POS command constants
private enum ESC {
    static let initialize  = Data([0x1B, 0x40])
    static let alignLeft   = Data([0x1B, 0x61, 0x00])
    static let alignCenter = Data([0x1B, 0x61, 0x01])
    static let boldOn      = Data([0x1B, 0x45, 0x01])
    static let boldOff     = Data([0x1B, 0x45, 0x00])
    static let doubleSize  = Data([0x1D, 0x21, 0x11])
    static let normalSize  = Data([0x1D, 0x21, 0x00])
    static func feed(_ lines: UInt8) -> Data { Data([0x1B, 0x64, lines]) }
    static let cut         = Data([0x1D, 0x56, 0x41, 0x00])
}

class PrinterService {

    // 42 columns for 80 mm paper (Epson TM-T88 default)
    private let paperWidth = 42

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

    // MARK: - Test print

    func sendTestPrint(to printer: POSPrinter, completion: @escaping (Bool, String) -> Void) {
        guard let connection = getConnection(for: printer) else {
            completion(false, "Invalid printer configuration.")
            return
        }

        var data = Data()
        data.append(ESC.initialize)
        data.append(ESC.alignCenter)
        data.append(ESC.boldOn)
        data.append(text("TEST PRINT\n"))
        data.append(ESC.boldOff)
        data.append(text("Printer is working correctly.\n"))
        data.append(ESC.feed(3))
        data.append(ESC.cut)

        connection.send(data: data) { success in
            DispatchQueue.main.async {
                completion(success, success ? "Test print sent successfully!" : "Failed to send test print.")
            }
        }
    }

    // MARK: - Receipt print

    func printReceipt(
        table: Table,
        tip: Double,
        businessName: String,
        currencyCode: String,
        to printer: POSPrinter,
        completion: @escaping (Bool, String) -> Void
    ) {
        guard let connection = getConnection(for: printer) else {
            completion(false, "Invalid printer configuration.")
            return
        }

        var data = Data()
        data.append(ESC.initialize)

        // ── Header ──────────────────────────────────────────────
        data.append(ESC.alignCenter)
        data.append(ESC.doubleSize)
        data.append(ESC.boldOn)
        data.append(text(businessName + "\n"))
        data.append(ESC.normalSize)
        data.append(ESC.boldOff)

        let dateFormatter = DateFormatter()
        dateFormatter.dateFormat = "dd/MM/yyyy  HH:mm"
        data.append(text(dateFormatter.string(from: Date()) + "\n"))
        data.append(text("Table \(table.id)\n"))
        data.append(text(divider() + "\n"))

        // ── Order items ─────────────────────────────────────────
        data.append(ESC.alignLeft)
        for item in table.items {
            let price = currency(item.finalPrice, code: currencyCode)
            data.append(text(columns(item.menuItem.name, price) + "\n"))
            for modifier in item.appliedModifiers {
                data.append(text("  + \(modifier.name)\n"))
            }
            if let note = item.note, !note.isEmpty {
                data.append(text("  (\(note))\n"))
            }
        }

        data.append(text(divider() + "\n"))

        // ── Tax breakdown ────────────────────────────────────────
        let alcoholicTax = table.items.reduce(0.0) { acc, item in
            guard item.menuItem.taxCategory == .alcoholic else { return acc }
            let p = item.finalPrice
            return acc + (p - p / (1 + item.menuItem.taxCategory.rate))
        }
        let nonAlcTax = table.items.reduce(0.0) { acc, item in
            guard item.menuItem.taxCategory == .nonAlcoholic else { return acc }
            let p = item.finalPrice
            return acc + (p - p / (1 + item.menuItem.taxCategory.rate))
        }
        let subtotal = table.total - alcoholicTax - nonAlcTax

        data.append(text(columns("Subtotal (excl. tax)", currency(subtotal, code: currencyCode)) + "\n"))
        if nonAlcTax > 0.005 {
            data.append(text(columns("Tax 9% (Non-Alc.)", currency(nonAlcTax, code: currencyCode)) + "\n"))
        }
        if alcoholicTax > 0.005 {
            data.append(text(columns("Tax 21% (Alcohol)", currency(alcoholicTax, code: currencyCode)) + "\n"))
        }
        data.append(text(columns("Order Total", currency(table.total, code: currencyCode)) + "\n"))

        if tip > 0.005 {
            data.append(text(columns("Tip", currency(tip, code: currencyCode)) + "\n"))
        }

        data.append(text(divider() + "\n"))
        data.append(ESC.boldOn)
        let grandTotal = table.total + tip
        data.append(text(columns("TOTAL", currency(grandTotal, code: currencyCode)) + "\n"))
        data.append(ESC.boldOff)
        data.append(text(divider() + "\n"))

        // ── Footer ───────────────────────────────────────────────
        data.append(ESC.alignCenter)
        data.append(text("Thank you for your visit!\n"))
        data.append(ESC.feed(4))
        data.append(ESC.cut)

        connection.send(data: data) { success in
            DispatchQueue.main.async {
                completion(success, success ? "Receipt printed." : "Failed to print receipt.")
            }
        }
    }

    // MARK: - ESC/POS helpers

    private func text(_ string: String) -> Data {
        Data(string.utf8)
    }

    private func divider() -> String {
        String(repeating: "-", count: paperWidth)
    }

    /// Two-column row with the right value right-padded to the paper edge.
    private func columns(_ left: String, _ right: String) -> String {
        let maxLeft = paperWidth - right.count - 1
        let truncated = left.count > maxLeft ? String(left.prefix(maxLeft)) : left
        let pad = paperWidth - truncated.count - right.count
        return truncated + String(repeating: " ", count: max(1, pad)) + right
    }

    private func currency(_ value: Double, code: String) -> String {
        let fmt = NumberFormatter()
        fmt.numberStyle = .currency
        fmt.currencyCode = code
        fmt.minimumFractionDigits = 2
        fmt.maximumFractionDigits = 2
        return fmt.string(from: NSNumber(value: value)) ?? String(format: "%.2f", value)
    }
}
