import SwiftUI

struct MenuView: View {
    @Environment(\.dismiss) private var dismiss
    @EnvironmentObject var posData: POSData
    @Binding var table: Table

    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    @State private var selectedCategory: MenuCategory? = nil
    @State private var selectedMenuItemForModifiers: MenuItem?

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
        .contextMenu {
            Button("Remove Item", role: .destructive) {
                withAnimation(.easeInOut(duration: 0.25)) {
                    if let index = table.items.firstIndex(where: { $0.id == item.id }) {
                        table.items.remove(at: index)
                    }
                }
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
