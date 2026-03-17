import SwiftUI

struct PrinterSettingsView: View {
    @EnvironmentObject var posData: POSData
    @State private var isAddingPrinter = false
    @State private var showAlert = false
    @State private var alertMessage = ""

    var body: some View {
        List {
            ForEach(posData.printers) { printer in
                HStack {
                    VStack(alignment: .leading) {
                        Text(printer.name)
                            .font(.headline)

                        switch printer.connectionType {
                        case .network:
                            Text(printer.ipAddress ?? "No IP Address")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        case .bluetooth:
                            Text("Bluetooth LE")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        case .epsonBluetooth:
                            Text("Epson \(printer.epsonSeries.displayName) (Bluetooth)")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                    }
                    Spacer()
                    Button("Test") {
                        let printerService = PrinterService()
                        printerService.sendTestPrint(to: printer) { success, message in
                            DispatchQueue.main.async {
                                self.alertMessage = message
                                self.showAlert = true
                            }
                        }
                    }
                    .buttonStyle(SecondaryButtonStyle())
                }
            }
            .onDelete(perform: deletePrinter)
        }
        .navigationTitle("Printer Settings")
        .toolbar {
            ToolbarItem(placement: .primaryAction) {
                Button(action: { isAddingPrinter = true }) {
                    Image(systemName: "plus")
                }
            }
        }
        .sheet(isPresented: $isAddingPrinter) {
            AddPrinterView()
        }
        .alert(isPresented: $showAlert) {
            Alert(title: Text("Test Print"), message: Text(alertMessage), dismissButton: .default(Text("OK")))
        }
    }

    private func deletePrinter(at offsets: IndexSet) {
        offsets.forEach { index in
            let printer = posData.printers[index]
            posData.removePrinter(printer)
        }
    }
}

struct AddPrinterView: View {
    @EnvironmentObject var posData: POSData
    @State private var name: String = ""
    @State private var connectionType: PrinterConnectionType = .network
    @State private var ipAddress: String = ""
    @State private var port: String = "9100"
    @State private var selectedBluetoothIdentifier: String?
    @State private var selectedBluetoothName: String?
    @State private var epsonSeries: EpsonPrinterSeries = .tmT88
    @State private var epsonBTAddress: String = ""
    @State private var isScanning = false
    @State private var isEpsonScanning = false

    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("Printer Details")) {
                    TextField("Printer Name (e.g., Receipt Printer)", text: $name)

                    Picker("Connection Type", selection: $connectionType) {
                        ForEach(PrinterConnectionType.allCases, id: \.self) { type in
                            Text(type.rawValue).tag(type)
                        }
                    }
                    .pickerStyle(SegmentedPickerStyle())
                }

                if connectionType == .network {
                    Section(header: Text("Network Settings")) {
                        TextField("IP Address (e.g., 192.168.1.100)", text: $ipAddress)
                            .keyboardType(.decimalPad)
                        TextField("Port", text: $port)
                            .keyboardType(.numberPad)
                    }
                } else if connectionType == .bluetooth {
                    Section(header: Text("Bluetooth LE Settings")) {
                        if let deviceName = selectedBluetoothName {
                            Text("Selected: \(deviceName)")
                        } else {
                            Text("No device selected.")
                        }
                        Button("Scan for LE Devices") {
                            isScanning = true
                        }
                    }
                } else if connectionType == .epsonBluetooth {
                    Section(header: Text("Epson Model")) {
                        Picker("Printer Series", selection: $epsonSeries) {
                            ForEach(EpsonPrinterSeries.allCases) { series in
                                Text(series.displayName).tag(series)
                            }
                        }
                    }
                    Section(header: Text("Epson Bluetooth")) {
                        if let deviceName = selectedBluetoothName {
                            Text("Selected: \(deviceName)")
                                .foregroundColor(.secondary)
                        } else {
                            Text("No device selected.")
                                .foregroundColor(.secondary)
                        }
                        TextField("Or enter BT address / device name", text: $epsonBTAddress)
                            .autocapitalization(.none)
                        Button("Scan for Devices") {
                            isEpsonScanning = true
                        }
                    }
                }

                Section {
                    Button("Add Printer") {
                        addPrinter()
                    }
                    .disabled(name.isEmpty)
                }
            }
            .navigationTitle("Add New Printer")
            .navigationBarItems(leading: Button("Cancel") {
                dismiss()
            })
            .sheet(isPresented: $isScanning) {
                BluetoothDeviceScannerView(
                    selectedPeripheralIdentifier: $selectedBluetoothIdentifier,
                    selectedPeripheralName: $selectedBluetoothName
                )
            }
            .sheet(isPresented: $isEpsonScanning) {
                BluetoothDeviceScannerView(
                    selectedPeripheralIdentifier: $selectedBluetoothIdentifier,
                    selectedPeripheralName: $selectedBluetoothName
                )
                .onDisappear {
                    // Populate the manual field if user selected via scanner
                    if let name = selectedBluetoothName, epsonBTAddress.isEmpty {
                        epsonBTAddress = name
                    }
                }
            }
        }
    }

    private func addPrinter() {
        var newPrinter: POSPrinter

        switch connectionType {
        case .network:
            guard !ipAddress.isEmpty, let portNumber = Int(port) else { return }
            newPrinter = POSPrinter(name: name, connectionType: .network, ipAddress: ipAddress, port: portNumber)
        case .bluetooth:
            guard let identifier = selectedBluetoothIdentifier else { return }
            newPrinter = POSPrinter(name: name, connectionType: .bluetooth, bluetoothIdentifier: identifier)
        case .epsonBluetooth:
            // Prefer scanned BT identifier; fall back to manually typed address/name
            let btID = selectedBluetoothName ?? epsonBTAddress
            guard !btID.isEmpty else { return }
            newPrinter = POSPrinter(
                name: name,
                connectionType: .epsonBluetooth,
                bluetoothIdentifier: btID,
                epsonSeries: epsonSeries
            )
        }

        posData.addPrinter(newPrinter)
        dismiss()
    }
}

struct PrinterSettingsView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            PrinterSettingsView()
                .environmentObject(POSData())
        }
    }
}
