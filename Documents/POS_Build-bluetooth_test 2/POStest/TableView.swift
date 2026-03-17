import SwiftUI

struct TableView: View {
    @EnvironmentObject var posData: POSData

    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    @State private var selectedTableID: Int?

    // State for opening a table
    @State private var tableToOpenID: Int?
    @State private var isOpeningTable = false
    @State private var newGuestCount: Int?
    @State private var newGroupType: GroupType?

    // State for closing a table
    @State private var showReceipt = false

    @State private var tableLayout: [TableLayoutInfo] = []
    @AppStorage("tableLayoutType") private var tableLayoutType: String = "Grid"

    let columns = [
        GridItem(.adaptive(minimum: 120))
    ]

    var body: some View {
        HStack(spacing: 0) {
            // Tables Grid/Layout
            tablesSection
            
            // Details Panel
            detailsPanel
        }
        .background(POSColors.backgroundGradient)
        .navigationTitle("Tables")
        .navigationBarTitleDisplayMode(.large)
        .toolbarBackground(POSColors.backgroundDark, for: .navigationBar)
        .toolbarBackground(.visible, for: .navigationBar)
        .toolbarColorScheme(.dark, for: .navigationBar)
        .sheet(isPresented: $isOpeningTable) {
            OpenTableView(
                guestCount: $newGuestCount,
                groupType: $newGroupType,
                onConfirm: { openTable() },
                onCancel: { isOpeningTable = false }
            )
        }
        .sheet(isPresented: $showReceipt) {
            if let selectedTableID = selectedTableID,
               let table = posData.tables.first(where: { $0.id == selectedTableID }) {
                ReceiptView(isPresented: $showReceipt, table: table)
            }
        }
        .onChange(of: showReceipt) {
            if !showReceipt {
                self.selectedTableID = nil
            }
        }
        .onAppear(perform: loadLayout)
        .preferredColorScheme(.dark)
    }
    
    // MARK: - Tables Section
    private var tablesSection: some View {
        VStack(spacing: 0) {
            // Header
            HStack {
                Text("Tables")
                    .font(.title.weight(.bold))
                    .foregroundColor(POSColors.textPrimary)
                
                Spacer()
                
                Text("\(posData.tables.filter { $0.openedAt != nil }.count) of \(posData.tables.count) occupied")
                    .font(.subheadline)
                    .foregroundColor(POSColors.textSecondary)
            }
            .padding(.horizontal, 20)
            .padding(.top, 20)
            
            if tableLayoutType == "Grid" {
                gridLayout
            } else {
                customLayout
            }
        }
        .frame(maxWidth: .infinity)
    }

    private var gridLayout: some View {
        ScrollView {
            LazyVGrid(columns: columns, spacing: 16) {
                ForEach(posData.tables) { table in
                    Button(action: {
                        handleTableTap(table: table)
                    }) {
                        Text("\(table.id)")
                    }
                    .buttonStyle(LiveTableButtonStyle(table: table, isSelected: selectedTableID == table.id))
                }
            }
            .padding(20)
        }
    }

    private var customLayout: some View {
        GeometryReader { geometry in
            ScrollView([.horizontal, .vertical]) {
                ZStack(alignment: .topLeading) {
                    Rectangle()
                        .fill(Color.clear)
                        .frame(width: 2000, height: 2000)

                    ForEach(posData.tables) { table in
                        if let layout = tableLayout.first(where: { $0.id == table.id }) {
                            Button(action: {
                                handleTableTap(table: table)
                            }) {
                                VStack(spacing: 6) {
                                    Text("\(table.id)")
                                        .font(.title2.weight(.medium))
                                        .foregroundColor(POSColors.textPrimary)

                                    if let openedAt = table.openedAt {
                                        LiveTimerView(startDate: openedAt)
                                            .font(.caption.weight(.medium))
                                            .foregroundColor(POSColors.textSecondary)
                                    }

                                    if let lastOrderItemAt = table.lastOrderItemAt {
                                        LiveTimerView(startDate: lastOrderItemAt)
                                            .font(.caption2)
                                            .foregroundColor(POSColors.textMuted)
                                    }
                                }
                                .frame(width: layout.width, height: layout.height)
                                .background(
                                    RoundedRectangle(cornerRadius: POSRadius.medium)
                                        .fill(tableBackgroundColor(for: table))
                                )
                                .overlay(
                                    RoundedRectangle(cornerRadius: POSRadius.medium)
                                        .stroke(
                                            selectedTableID == table.id ? POSColors.goldPrimary : POSColors.goldSubtle.opacity(0.3),
                                            lineWidth: selectedTableID == table.id ? 3 : 1
                                        )
                                )
                                .shadow(
                                    color: POSShadows.card.color,
                                    radius: POSShadows.card.radius,
                                    x: POSShadows.card.x,
                                    y: POSShadows.card.y
                                )
                            }
                            .position(x: layout.x, y: layout.y)
                        }
                    }
                }
            }
        }
    }
    
    private func tableBackgroundColor(for table: Table) -> Color {
        guard let openedAt = table.openedAt else {
            return POSColors.backgroundMedium
        }

        let duration = Date().timeIntervalSince(openedAt)
        let thirtyMinutes = 30.0 * 60.0
        let percentage = min(duration / thirtyMinutes, 1.0)

        // Sophisticated color transition from green to amber to red
        if percentage < 0.5 {
            // Green to amber transition
            let localPercentage = percentage * 2
            return Color(
                red: 0.2 + (localPercentage * 0.6),   // 0.2 to 0.8
                green: 0.8 - (localPercentage * 0.1), // 0.8 to 0.7
                blue: 0.2
            )
        } else {
            // Amber to red transition
            let localPercentage = (percentage - 0.5) * 2
            return Color(
                red: 0.8 + (localPercentage * 0.2),   // 0.8 to 1.0
                green: 0.7 - (localPercentage * 0.5), // 0.7 to 0.2
                blue: 0.2 - (localPercentage * 0.1)   // 0.2 to 0.1
            )
        }
    }

    // MARK: - Details Panel
    private var detailsPanel: some View {
        VStack(spacing: 0) {
            if let selectedTableID = selectedTableID,
               let selectedTableIndex = posData.tables.firstIndex(where: { $0.id == selectedTableID }) {
                
                let table = posData.tables[selectedTableIndex]
                
                tableDetailsContent(table: table, tableIndex: selectedTableIndex)
                
            } else {
                emptySelectionState
            }
        }
        .frame(width: 380)
        .background(POSColors.backgroundDark)
        .overlay(
            Rectangle()
                .fill(POSColors.goldSubtle.opacity(0.2))
                .frame(width: 1),
            alignment: .leading
        )
    }
    
    private func tableDetailsContent(table: Table, tableIndex: Int) -> some View {
        VStack(spacing: 24) {
            // Table Header
            VStack(spacing: 12) {
                Text("Table \(table.id)")
                    .font(.system(size: 42, weight: .bold, design: .rounded))
                    .foregroundColor(POSColors.textPrimary)
                
                if table.openedAt != nil {
                    HStack(spacing: 16) {
                        infoItem(icon: "person.2.fill", text: "\(table.guestCount ?? 0) Guests")
                        infoItem(icon: "tag.fill", text: table.groupType?.rawValue ?? "N/A")
                    }
                    
                    infoItem(
                        icon: "clock.fill",
                        text: "Opened: \(itemFormatter.string(from: table.openedAt ?? Date()))"
                    )
                }
            }
            .padding(.horizontal, 20)
            .padding(.top, 24)
            
            if table.openedAt != nil {
                // Order Summary
                VStack(spacing: 16) {
                    HStack {
                        Text("Order Summary")
                            .font(.headline.weight(.semibold))
                            .foregroundColor(POSColors.textPrimary)
                        Spacer()
                    }
                    
                    VStack(spacing: 8) {
                        summaryRow(label: "Items", value: "\(table.itemCount)")
                        summaryRow(label: "Total", value: table.total.formatted(.currency(code: currencyCode)), isTotal: true)
                    }
                }
                .padding(20)
                .background(
                    RoundedRectangle(cornerRadius: POSRadius.medium)
                        .fill(POSColors.backgroundMedium)
                        .shadow(
                            color: POSShadows.card.color,
                            radius: POSShadows.card.radius,
                            x: POSShadows.card.x,
                            y: POSShadows.card.y
                        )
                )
                .padding(.horizontal, 20)
                
                Spacer()
                
                // Action Buttons
                VStack(spacing: 12) {
                    NavigationLink(destination: MenuView(table: $posData.tables[tableIndex])) {
                        Label("Add Items", systemImage: "plus.circle.fill")
                            .font(.title2.weight(.semibold))
                            .foregroundColor(POSColors.textPrimary)
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 16)
                    }
                    .buttonStyle(SecondaryButtonStyle())
                    
                    Button("Close & Pay") {
                        showReceipt = true
                    }
                    .buttonStyle(PrimaryButtonStyle())
                }
                .padding(.horizontal, 20)
                .padding(.bottom, 20)
            } else {
                Spacer()
                
                VStack(spacing: 16) {
                    Image(systemName: "table.badge.more.fill")
                        .font(.system(size: 60))
                        .foregroundColor(POSColors.goldPrimary)
                    
                    Text("Table Available")
                        .font(.title2.weight(.semibold))
                        .foregroundColor(POSColors.textPrimary)
                    
                    Text("Tap to open table")
                        .font(.subheadline)
                        .foregroundColor(POSColors.textSecondary)
                }
                
                Spacer()
            }
        }
    }
    
    private var emptySelectionState: some View {
        VStack(spacing: 16) {
            Spacer()
            
            Image(systemName: "hand.tap.fill")
                .font(.system(size: 60))
                .foregroundColor(POSColors.goldPrimary.opacity(0.6))
            
            Text("Select a Table")
                .font(.title2.weight(.semibold))
                .foregroundColor(POSColors.textSecondary)
            
            Text("Choose a table to view details")
                .font(.subheadline)
                .foregroundColor(POSColors.textMuted)
            
            Spacer()
        }
    }
    
    // MARK: - Helper Views
    private func infoItem(icon: String, text: String) -> some View {
        HStack(spacing: 8) {
            Image(systemName: icon)
                .font(.subheadline)
                .foregroundColor(POSColors.goldPrimary)
                .frame(width: 16)
            
            Text(text)
                .font(.subheadline.weight(.medium))
                .foregroundColor(POSColors.textSecondary)
        }
    }
    
    private func summaryRow(label: String, value: String, isTotal: Bool = false) -> some View {
        HStack {
            Text(label)
                .font(isTotal ? .headline.weight(.semibold) : .body.weight(.medium))
                .foregroundColor(isTotal ? POSColors.textPrimary : POSColors.textSecondary)
            
            Spacer()
            
            Text(value)
                .font(isTotal ? .headline.weight(.bold) : .body.weight(.medium))
                .foregroundColor(isTotal ? POSColors.goldPrimary : POSColors.textSecondary)
        }
    }

    // MARK: - Helper Methods
    private func loadLayout() {
        if let data = UserDefaults.standard.data(forKey: "tableLayoutData") {
            let decoder = JSONDecoder()
            if let decoded = try? decoder.decode([TableLayoutInfo].self, from: data) {
                self.tableLayout = decoded
                return
            }
        }
    }

    private func handleTableTap(table: Table) {
        if table.openedAt == nil {
            self.tableToOpenID = table.id
            self.newGuestCount = nil
            self.newGroupType = nil
            self.isOpeningTable = true
        } else {
            self.selectedTableID = table.id
        }
    }

    private func openTable() {
        guard let guestCount = newGuestCount,
              let groupType = newGroupType,
              let tableID = tableToOpenID else {
            return
        }

        posData.openTable(id: tableID, guestCount: guestCount, groupType: groupType)
        selectedTableID = tableID
        isOpeningTable = false
    }
}

private let itemFormatter: DateFormatter = {
    let formatter = DateFormatter()
    formatter.dateStyle = .none
    formatter.timeStyle = .short
    return formatter
}()

struct TableView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            TableView()
        }
        .environmentObject(POSData())
        .preferredColorScheme(.dark)
    }
}
