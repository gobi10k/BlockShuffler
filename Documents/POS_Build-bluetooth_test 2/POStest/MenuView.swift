import SwiftUI

struct MenuView: View {
    @Environment(\.dismiss) private var dismiss
    @EnvironmentObject var posData: POSData
    @Binding var table: Table

    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    @State private var selectedCategory: MenuCategory? = nil
    @State private var selectedMenuItemForModifiers: MenuItem?

    // Item note editing
    @State private var itemForNote: OrderItem?
    @State private var noteText: String = ""

    // Void with reason
    @State private var itemToVoid: OrderItem?
    @State private var selectedVoidReason: VoidReason = .mistake
    @State private var showVoidSheet = false

    let columns = [
        GridItem(.adaptive(minimum: 150), spacing: 16)
    ]

    // MARK: - Filtered items

    private var filteredItems: [MenuItem] {
        posData.menuItems.filter { item in
            item.isAvailable &&
            (selectedCategory == nil || item.category == selectedCategory)
        }
    }

    var body: some View {
        HStack(spacing: 0) {
            orderDetailsPanel
            menuItemsPanel
        }
        .background(POSColors.backgroundGradient)
        .navigationTitle("Table \(table.id)")
        .navigationBarTitleDisplayMode(.inline)
        .toolbarBackground(POSColors.backgroundDark, for: .navigationBar)
        .toolbarBackground(.visible, for: .navigationBar)
        .toolbarColorScheme(.dark, for: .navigationBar)
    }

    // MARK: - Order Details Panel

    private var orderDetailsPanel: some View {
        VStack(spacing: 0) {
            tableHeader
            orderItemsList
            orderSummary
            actionButtons
        }
        .frame(width: 340)
        .background(POSColors.backgroundDark)
        .overlay(
            Rectangle()
                .fill(POSColors.goldSubtle.opacity(0.2))
                .frame(width: 1),
            alignment: .trailing
        )
    }

    private var tableHeader: some View {
        VStack(spacing: 6) {
            Text("Table \(table.id)")
                .font(.title.weight(.bold))
                .foregroundColor(POSColors.textPrimary)

            if let guestCount = table.guestCount {
                Text("\(guestCount) guests")
                    .font(.subheadline.weight(.medium))
                    .foregroundColor(POSColors.goldPrimary)
            }

            if let openedAt = table.openedAt {
                Text("Opened \(openedAt, format: .dateTime.hour().minute())")
                    .font(.caption)
                    .foregroundColor(POSColors.textMuted)
            }
        }
        .padding(.vertical, 20)
        .padding(.horizontal, 16)
    }

    private var orderItemsList: some View {
        ScrollView {
            LazyVStack(spacing: 8) {
                ForEach(table.items) { item in
                    orderItemRow(item)
                }
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 4)
        }
        .frame(maxHeight: .infinity)
    }

    private func orderItemRow(_ item: OrderItem) -> some View {
        HStack(alignment: .top, spacing: 10) {
            // Category colour strip
            RoundedRectangle(cornerRadius: 2)
                .fill(item.menuItem.category.color)
                .frame(width: 3)

            VStack(alignment: .leading, spacing: 3) {
                Text(item.menuItem.name)
                    .font(.subheadline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                    .lineLimit(1)

                if !item.appliedModifiers.isEmpty {
                    Text(item.appliedModifiers.map { $0.name }.joined(separator: ", "))
                        .font(.caption)
                        .foregroundColor(POSColors.goldPrimary)
                }

                if let note = item.note, !note.isEmpty {
                    Label(note, systemImage: "note.text")
                        .font(.caption)
                        .foregroundColor(POSColors.textMuted)
                        .lineLimit(1)
                } else {
                    Text("Add note…")
                        .font(.caption)
                        .foregroundColor(POSColors.textDisabled)
                }
            }

            Spacer()

            Text(item.finalPrice, format: .currency(code: currencyCode))
                .font(.subheadline.weight(.semibold))
                .foregroundColor(POSColors.goldPrimary)
        }
        .padding(.vertical, 10)
        .padding(.horizontal, 12)
        .background(
            RoundedRectangle(cornerRadius: POSRadius.medium)
                .fill(POSColors.backgroundMedium)
        )
        .onTapGesture {
            noteText = item.note ?? ""
            itemForNote = item
        }
        .contextMenu {
            Button {
                noteText = item.note ?? ""
                itemForNote = item
            } label: {
                Label("Add / Edit Note", systemImage: "note.text")
            }

            Divider()

            Button(role: .destructive) {
                itemToVoid = item
                selectedVoidReason = .mistake
                showVoidSheet = true
            } label: {
                Label("Void Item…", systemImage: "trash")
            }
        }
    }

    private var orderSummary: some View {
        VStack(spacing: 10) {
            Divider()
                .background(POSColors.goldSubtle.opacity(0.3))

            VStack(spacing: 6) {
                summaryRow(label: "Subtotal", value: table.subtotal.formatted(.currency(code: currencyCode)))
                summaryRow(label: "Tax", value: table.tax.formatted(.currency(code: currencyCode)))

                Divider().background(POSColors.goldSubtle.opacity(0.2))

                HStack {
                    Text("Total")
                        .font(.title3.weight(.bold))
                        .foregroundColor(POSColors.textPrimary)
                    Spacer()
                    Text(table.total.formatted(.currency(code: currencyCode)))
                        .font(.title3.weight(.bold))
                        .foregroundColor(POSColors.goldPrimary)
                }
            }
            .padding(.horizontal, 16)
        }
        .padding(.vertical, 12)
    }

    private var actionButtons: some View {
        VStack(spacing: 10) {
            Button("Done") { dismiss() }
                .buttonStyle(PrimaryButtonStyle())
        }
        .padding(.horizontal, 16)
        .padding(.bottom, 16)
        // Note editing sheet
        .sheet(item: $itemForNote) { item in
            itemNoteSheet(for: item)
        }
        // Void with reason sheet
        .sheet(isPresented: $showVoidSheet) {
            voidReasonSheet
        }
    }

    private func itemNoteSheet(for item: OrderItem) -> some View {
        NavigationStack {
            VStack(spacing: 24) {
                VStack(spacing: 6) {
                    Image(systemName: item.menuItem.category.icon)
                        .font(.title)
                        .foregroundColor(item.menuItem.category.color)
                    Text(item.menuItem.name)
                        .font(.title2.weight(.bold))
                        .foregroundColor(POSColors.textPrimary)
                }
                .frame(maxWidth: .infinity)
                .padding(.top, 8)

                VStack(alignment: .leading, spacing: 8) {
                    Text("Note")
                        .font(.headline.weight(.semibold))
                        .foregroundColor(POSColors.textPrimary)

                    TextField("e.g. no ice, extra lime, oat milk…", text: $noteText, axis: .vertical)
                        .lineLimit(3, reservesSpace: true)
                        .font(.body)
                        .foregroundColor(POSColors.textPrimary)
                        .padding(12)
                        .background(
                            RoundedRectangle(cornerRadius: POSRadius.small)
                                .fill(POSColors.backgroundLight.opacity(0.3))
                        )
                        .autocorrectionDisabled()
                }
                .padding(.horizontal, 20)

                // Quick-note chips
                let suggestions = ["No ice", "Extra ice", "On the rocks", "Straight up", "Extra dry", "Oat milk", "No garnish", "Double"]
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 8) {
                        ForEach(suggestions, id: \.self) { suggestion in
                            Button(suggestion) {
                                if noteText.isEmpty {
                                    noteText = suggestion
                                } else {
                                    noteText += ", \(suggestion)"
                                }
                            }
                            .font(.caption.weight(.semibold))
                            .foregroundColor(POSColors.goldPrimary)
                            .padding(.vertical, 6)
                            .padding(.horizontal, 12)
                            .background(Capsule().fill(POSColors.goldPrimary.opacity(0.12)))
                        }
                    }
                    .padding(.horizontal, 20)
                }

                Spacer()
            }
            .background(POSColors.backgroundGradient)
            .navigationTitle("Item Note")
            .navigationBarTitleDisplayMode(.inline)
            .toolbarBackground(POSColors.backgroundDark, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbarColorScheme(.dark, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { itemForNote = nil }
                        .foregroundColor(POSColors.goldPrimary)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Save") {
                        if let index = table.items.firstIndex(where: { $0.id == item.id }) {
                            table.items[index].note = noteText.isEmpty ? nil : noteText
                        }
                        itemForNote = nil
                    }
                    .foregroundColor(POSColors.goldPrimary)
                    .fontWeight(.semibold)
                }
            }
        }
        .preferredColorScheme(.dark)
    }

    private var voidReasonSheet: some View {
        NavigationStack {
            VStack(spacing: 0) {
                if let item = itemToVoid {
                    // Item summary
                    HStack(spacing: 12) {
                        Image(systemName: item.menuItem.category.icon)
                            .foregroundColor(item.menuItem.category.color)
                        Text(item.menuItem.name)
                            .font(.headline.weight(.semibold))
                            .foregroundColor(POSColors.textPrimary)
                        Spacer()
                        Text(item.finalPrice, format: .currency(code: currencyCode))
                            .font(.headline.weight(.bold))
                            .foregroundColor(POSColors.error)
                    }
                    .padding(16)
                    .background(POSColors.backgroundMedium)
                }

                // Reason list
                ScrollView {
                    LazyVStack(spacing: 10) {
                        ForEach(VoidReason.allCases) { reason in
                            let isSelected = selectedVoidReason == reason
                            Button {
                                selectedVoidReason = reason
                            } label: {
                                HStack(spacing: 14) {
                                    Image(systemName: reason.icon)
                                        .font(.title3)
                                        .foregroundColor(isSelected ? POSColors.backgroundDark : POSColors.error)
                                        .frame(width: 36, height: 36)
                                        .background(Circle().fill(isSelected ? POSColors.error : POSColors.error.opacity(0.12)))

                                    Text(reason.rawValue)
                                        .font(.headline.weight(.medium))
                                        .foregroundColor(POSColors.textPrimary)

                                    Spacer()

                                    if isSelected {
                                        Image(systemName: "checkmark.circle.fill")
                                            .foregroundColor(POSColors.error)
                                    }
                                }
                                .padding(.vertical, 12)
                                .padding(.horizontal, 16)
                                .background(
                                    RoundedRectangle(cornerRadius: POSRadius.medium)
                                        .fill(isSelected ? POSColors.error.opacity(0.1) : POSColors.backgroundMedium)
                                        .overlay(
                                            RoundedRectangle(cornerRadius: POSRadius.medium)
                                                .stroke(isSelected ? POSColors.error.opacity(0.4) : Color.clear, lineWidth: 1)
                                        )
                                )
                            }
                            .buttonStyle(.plain)
                            .animation(.easeInOut(duration: 0.15), value: isSelected)
                        }
                    }
                    .padding(16)
                }

                // Confirm button
                Button("Void Item") {
                    if let item = itemToVoid,
                       let index = table.items.firstIndex(where: { $0.id == item.id }) {
                        withAnimation(.easeInOut(duration: 0.25)) {
                            table.items.remove(at: index)
                        }
                    }
                    itemToVoid = nil
                    showVoidSheet = false
                }
                .buttonStyle(PrimaryButtonStyle(isDestructive: true))
                .padding(16)
            }
            .background(POSColors.backgroundGradient)
            .navigationTitle("Void Item")
            .navigationBarTitleDisplayMode(.inline)
            .toolbarBackground(POSColors.backgroundDark, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbarColorScheme(.dark, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") {
                        itemToVoid = nil
                        showVoidSheet = false
                    }
                    .foregroundColor(POSColors.goldPrimary)
                }
            }
        }
        .preferredColorScheme(.dark)
    }

    private func summaryRow(label: String, value: String) -> some View {
        HStack {
            Text(label)
                .font(.subheadline.weight(.medium))
                .foregroundColor(POSColors.textSecondary)
            Spacer()
            Text(value)
                .font(.subheadline.weight(.medium))
                .foregroundColor(POSColors.textSecondary)
        }
    }

    // MARK: - Menu Items Panel

    private var menuItemsPanel: some View {
        VStack(spacing: 0) {
            categoryFilterBar
            menuGrid
        }
        .background(POSColors.backgroundDark.opacity(0.5))
        .sheet(item: $selectedMenuItemForModifiers) { menuItem in
            ModifierSelectionView(menuItem: menuItem) { selectedModifiers in
                addItemToOrder(menuItem: menuItem, modifiers: selectedModifiers)
            }
        }
    }

    // Horizontal scrolling category tabs
    private var categoryFilterBar: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 8) {
                // "All" pill
                categoryPill(
                    label: "All",
                    icon: "square.grid.2x2",
                    color: POSColors.goldPrimary,
                    isSelected: selectedCategory == nil
                ) {
                    withAnimation(.easeInOut(duration: 0.2)) { selectedCategory = nil }
                }

                ForEach(MenuCategory.allCases) { category in
                    let count = posData.menuItems.filter {
                        $0.isAvailable && $0.category == category
                    }.count

                    if count > 0 {
                        categoryPill(
                            label: category.rawValue,
                            icon: category.icon,
                            color: category.color,
                            isSelected: selectedCategory == category
                        ) {
                            withAnimation(.easeInOut(duration: 0.2)) {
                                selectedCategory = category
                            }
                        }
                    }
                }
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 12)
        }
        .background(POSColors.backgroundDark)
        .overlay(
            Rectangle()
                .fill(POSColors.goldSubtle.opacity(0.15))
                .frame(height: 1),
            alignment: .bottom
        )
    }

    private func categoryPill(
        label: String,
        icon: String,
        color: Color,
        isSelected: Bool,
        action: @escaping () -> Void
    ) -> some View {
        Button(action: action) {
            HStack(spacing: 6) {
                Image(systemName: icon)
                    .font(.caption.weight(.semibold))
                Text(label)
                    .font(.subheadline.weight(.semibold))
            }
            .foregroundColor(isSelected ? POSColors.backgroundDark : color)
            .padding(.vertical, 8)
            .padding(.horizontal, 14)
            .background(
                Capsule()
                    .fill(isSelected ? color : color.opacity(0.12))
            )
        }
    }

    private var menuGrid: some View {
        ScrollView {
            if filteredItems.isEmpty {
                emptyState
            } else {
                LazyVGrid(columns: columns, spacing: 14) {
                    ForEach(filteredItems) { menuItem in
                        menuItemCard(menuItem)
                    }
                }
                .padding(16)
            }
        }
    }

    private var emptyState: some View {
        VStack(spacing: 12) {
            Image(systemName: "wineglass")
                .font(.system(size: 48))
                .foregroundColor(POSColors.goldPrimary.opacity(0.4))
            Text("No items in this category")
                .font(.subheadline)
                .foregroundColor(POSColors.textMuted)
        }
        .frame(maxWidth: .infinity)
        .padding(.top, 60)
    }

    private func menuItemCard(_ menuItem: MenuItem) -> some View {
        Button(action: {
            if menuItem.hasModifiers {
                selectedMenuItemForModifiers = menuItem
            } else {
                addItemToOrder(menuItem: menuItem)
            }
        }) {
            VStack(spacing: 0) {
                // Colour band at top
                Rectangle()
                    .fill(menuItem.category.color.opacity(0.8))
                    .frame(height: 4)

                VStack(spacing: 8) {
                    // Icon
                    Image(systemName: menuItem.category.icon)
                        .font(.title2)
                        .foregroundColor(menuItem.category.color)
                        .frame(height: 32)

                    // Name
                    Text(menuItem.name)
                        .font(.subheadline.weight(.semibold))
                        .foregroundColor(POSColors.textPrimary)
                        .multilineTextAlignment(.center)
                        .lineLimit(2)
                        .fixedSize(horizontal: false, vertical: true)

                    // Price
                    Text(menuItem.price, format: .currency(code: currencyCode))
                        .font(.headline.weight(.bold))
                        .foregroundColor(POSColors.goldPrimary)

                    // Modifier indicator
                    if menuItem.hasModifiers {
                        Text("Customisable")
                            .font(.caption2.weight(.medium))
                            .foregroundColor(POSColors.textMuted)
                    }
                }
                .padding(.horizontal, 10)
                .padding(.vertical, 12)
            }
            .frame(minHeight: 130)
        }
        .buttonStyle(MenuItemCardStyle())
    }

    // MARK: - Helper

    private func addItemToOrder(menuItem: MenuItem, modifiers: [MenuItemModifier] = []) {
        withAnimation(.easeInOut(duration: 0.25)) {
            let orderItem = OrderItem(menuItem: menuItem, timestamp: Date(), appliedModifiers: modifiers)
            table.items.append(orderItem)
            table.lastOrderItemAt = Date()
        }
    }
}

// MARK: - Preview

#Preview {
    @Previewable @State var dummyTable = Table(
        id: 5,
        items: [],
        guestCount: 3,
        openedAt: Date().addingTimeInterval(-1200)
    )

    NavigationStack {
        MenuView(table: $dummyTable)
            .environmentObject(POSData())
            .preferredColorScheme(.dark)
    }
}
