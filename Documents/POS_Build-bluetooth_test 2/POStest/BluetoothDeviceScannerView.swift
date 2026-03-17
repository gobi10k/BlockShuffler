import SwiftUI
import CoreBluetooth

// MARK: - Scanner ViewModel

@MainActor
final class BluetoothScanner: NSObject, ObservableObject {
    @Published var discoveredDevices: [DiscoveredDevice] = []
    @Published var isScanning = false
    @Published var bluetoothState: CBManagerState = .unknown

    struct DiscoveredDevice: Identifiable {
        let id: String          // peripheral UUID string
        let name: String
        let rssi: Int
    }

    private var centralManager: CBCentralManager!
    private var seenIds: Set<String> = []

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    func startScan() {
        guard centralManager.state == .poweredOn else { return }
        discoveredDevices.removeAll()
        seenIds.removeAll()
        isScanning = true
        centralManager.scanForPeripherals(withServices: nil, options: [
            CBCentralManagerScanOptionAllowDuplicatesKey: false
        ])
    }

    func stopScan() {
        centralManager.stopScan()
        isScanning = false
    }
}

extension BluetoothScanner: CBCentralManagerDelegate {
    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        Task { @MainActor in
            bluetoothState = central.state
            if central.state == .poweredOn {
                startScan()
            } else {
                isScanning = false
            }
        }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        let idString = peripheral.identifier.uuidString
        let name = peripheral.name
            ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String
            ?? "Unknown Device"
        let rssi = RSSI.intValue

        Task { @MainActor in
            guard !seenIds.contains(idString) else { return }
            seenIds.insert(idString)
            discoveredDevices.append(
                BluetoothScanner.DiscoveredDevice(id: idString, name: name, rssi: rssi)
            )
            // Sort: named devices first, then by signal strength
            discoveredDevices.sort {
                let aUnknown = $0.name == "Unknown Device"
                let bUnknown = $1.name == "Unknown Device"
                if aUnknown != bUnknown { return !aUnknown }
                return $0.rssi > $1.rssi
            }
        }
    }
}

// MARK: - View

struct BluetoothDeviceScannerView: View {
    @Binding var selectedPeripheralIdentifier: String?
    @Binding var selectedPeripheralName: String?

    @StateObject private var scanner = BluetoothScanner()
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            Group {
                switch scanner.bluetoothState {
                case .poweredOn:
                    deviceList
                case .poweredOff:
                    stateMessage(
                        icon: "bluetooth.slash",
                        title: "Bluetooth is Off",
                        message: "Turn on Bluetooth in Settings to scan for devices."
                    )
                case .unauthorized:
                    stateMessage(
                        icon: "lock.slash",
                        title: "Bluetooth Access Denied",
                        message: "Grant Bluetooth permission to this app in Settings."
                    )
                case .unsupported:
                    stateMessage(
                        icon: "xmark.circle",
                        title: "Not Supported",
                        message: "This device does not support Bluetooth LE."
                    )
                default:
                    stateMessage(
                        icon: "antenna.radiowaves.left.and.right",
                        title: "Initialising…",
                        message: "Waiting for Bluetooth."
                    )
                }
            }
            .background(POSColors.backgroundGradient)
            .navigationTitle("Select BLE Printer")
            .navigationBarTitleDisplayMode(.inline)
            .toolbarBackground(POSColors.backgroundDark, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbarColorScheme(.dark, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") {
                        scanner.stopScan()
                        dismiss()
                    }
                    .foregroundColor(POSColors.goldPrimary)
                }

                ToolbarItem(placement: .primaryAction) {
                    if scanner.isScanning {
                        ProgressView()
                            .tint(POSColors.goldPrimary)
                    } else {
                        Button("Rescan") { scanner.startScan() }
                            .foregroundColor(POSColors.goldPrimary)
                    }
                }
            }
        }
        .preferredColorScheme(.dark)
        .onDisappear { scanner.stopScan() }
    }

    // MARK: - Device list

    private var deviceList: some View {
        ScrollView {
            LazyVStack(spacing: 10) {
                if scanner.discoveredDevices.isEmpty {
                    scanningPlaceholder
                } else {
                    ForEach(scanner.discoveredDevices) { device in
                        deviceRow(device)
                    }
                }
            }
            .padding(16)
        }
    }

    private var scanningPlaceholder: some View {
        VStack(spacing: 14) {
            ProgressView()
                .tint(POSColors.goldPrimary)
                .scaleEffect(1.4)
            Text("Scanning for devices…")
                .font(.subheadline)
                .foregroundColor(POSColors.textMuted)
        }
        .frame(maxWidth: .infinity)
        .padding(.top, 60)
    }

    private func deviceRow(_ device: BluetoothScanner.DiscoveredDevice) -> some View {
        let isSelected = selectedPeripheralIdentifier == device.id

        return Button {
            selectedPeripheralIdentifier = device.id
            selectedPeripheralName = device.name
            scanner.stopScan()
            dismiss()
        } label: {
            HStack(spacing: 14) {
                Image(systemName: "printer.fill")
                    .font(.title3)
                    .foregroundColor(isSelected ? POSColors.backgroundDark : POSColors.goldPrimary)
                    .frame(width: 36, height: 36)
                    .background(
                        Circle().fill(
                            isSelected ? POSColors.goldPrimary : POSColors.goldPrimary.opacity(0.15)
                        )
                    )

                VStack(alignment: .leading, spacing: 3) {
                    Text(device.name)
                        .font(.headline.weight(.semibold))
                        .foregroundColor(POSColors.textPrimary)

                    Text(device.id)
                        .font(.caption2)
                        .foregroundColor(POSColors.textMuted)
                        .lineLimit(1)
                }

                Spacer()

                signalIcon(rssi: device.rssi)

                if isSelected {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundColor(POSColors.goldPrimary)
                }
            }
            .padding(.vertical, 12)
            .padding(.horizontal, 16)
            .background(
                RoundedRectangle(cornerRadius: POSRadius.medium)
                    .fill(isSelected ? POSColors.goldPrimary.opacity(0.1) : POSColors.backgroundMedium)
                    .overlay(
                        RoundedRectangle(cornerRadius: POSRadius.medium)
                            .stroke(
                                isSelected ? POSColors.goldPrimary.opacity(0.5) : Color.clear,
                                lineWidth: 1
                            )
                    )
            )
        }
        .buttonStyle(.plain)
    }

    private func signalIcon(rssi: Int) -> some View {
        let (icon, color): (String, Color) = {
            switch rssi {
            case -60...: return ("wifi",             POSColors.success)
            case -80...: return ("wifi",             POSColors.warning)
            default:     return ("wifi.exclamationmark", POSColors.error)
            }
        }()
        return Image(systemName: icon)
            .font(.caption)
            .foregroundColor(color)
    }

    // MARK: - State message

    private func stateMessage(icon: String, title: String, message: String) -> some View {
        VStack(spacing: 16) {
            Spacer()
            Image(systemName: icon)
                .font(.system(size: 56))
                .foregroundColor(POSColors.goldPrimary.opacity(0.5))
            Text(title)
                .font(.title2.weight(.semibold))
                .foregroundColor(POSColors.textPrimary)
            Text(message)
                .font(.subheadline)
                .foregroundColor(POSColors.textSecondary)
                .multilineTextAlignment(.center)
                .padding(.horizontal, 32)
            Spacer()
        }
        .frame(maxWidth: .infinity)
    }
}
