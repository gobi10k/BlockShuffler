import SwiftUI

struct MenuSettingsView: View {
    @EnvironmentObject var posData: POSData
    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    @State private var newItemName: String = ""
    @State private var newItemPrice: String = ""
    @State private var newItemCost: String = ""
    @State private var newItemTaxBandId: UUID?
    @State private var newItemCategory: MenuCategory = .other
    @State private var showingAddItem = false
    @State private var selectedMenuItem: MenuItem?
    @State private var filterCategory: MenuCategory? = nil

    private var displayedItems: [MenuItem] {
        if let cat = filterCategory {
            return posData.menuItems.filter { $0.category == cat }
        }
        return posData.menuItems
    }

    var body: some View {
        VStack(spacing: 0) {
            // Category filter strip
            categoryFilter

            // Menu Items List
            if displayedItems.isEmpty {
                emptyStateView
                    .frame(maxHeight: .infinity)
            } else {
                ScrollView {
                    LazyVStack(spacing: 12) {
                        ForEach(displayedItems) { menuItem in
                            menuItemCard(menuItem)
                        }
                    }
                    .padding()
                }
            }

            // Add Item Button
            Button("Add Menu Item") {
                showingAddItem = true
            }
            .buttonStyle(PrimaryButtonStyle())
            .padding()
        }
        .background(POSColors.backgroundGradient)
        .navigationTitle("Menu Settings")
        .navigationBarTitleDisplayMode(.large)
        .sheet(isPresented: $showingAddItem) {
            addItemSheet
        }
        .sheet(item: $selectedMenuItem) { menuItem in
            if let index = posData.menuItems.firstIndex(where: { $0.id == menuItem.id }) {
                ModifierManagementView(modifiers: $posData.menuItems[index].availableModifiers)
            } else {
                Text("Error: Menu item not found.")
            }
        }
        .preferredColorScheme(.dark)
    }
    
    private var categoryFilter: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 8) {
                filterPill(label: "All", icon: "square.grid.2x2", color: POSColors.goldPrimary, isSelected: filterCategory == nil) {
                    filterCategory = nil
                }
                ForEach(MenuCategory.allCases) { category in
                    filterPill(
                        label: category.rawValue,
                        icon: category.icon,
                        color: category.color,
                        isSelected: filterCategory == category
                    ) {
                        filterCategory = (filterCategory == category) ? nil : category
                    }
                }
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 10)
        }
        .background(POSColors.backgroundDark)
    }

    private func filterPill(label: String, icon: String, color: Color, isSelected: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack(spacing: 5) {
                Image(systemName: icon).font(.caption)
                Text(label).font(.caption.weight(.semibold))
            }
            .foregroundColor(isSelected ? POSColors.backgroundDark : color)
            .padding(.vertical, 6)
            .padding(.horizontal, 12)
            .background(Capsule().fill(isSelected ? color : color.opacity(0.15)))
        }
    }

    private var emptyStateView: some View {
        VStack(spacing: 16) {
            Image(systemName: "menucard")
                .font(.system(size: 60))
                .foregroundColor(POSColors.goldPrimary.opacity(0.6))
            
            Text("No Menu Items")
                .font(.title2.weight(.semibold))
                .foregroundColor(POSColors.textPrimary)
            
            Text("Add your first menu item to get started")
                .font(.subheadline)
                .foregroundColor(POSColors.textSecondary)
        }
        .padding(40)
    }
    
    private func menuItemCard(_ menuItem: MenuItem) -> some View {
        VStack(spacing: 12) {
            HStack(spacing: 12) {
                // Category colour dot
                Circle()
                    .fill(menuItem.category.color)
                    .frame(width: 10, height: 10)

                VStack(alignment: .leading, spacing: 3) {
                    Text(menuItem.name)
                        .font(.headline.weight(.semibold))
                        .foregroundColor(menuItem.isAvailable ? POSColors.textPrimary : POSColors.textDisabled)

                    HStack(spacing: 8) {
                        Text(menuItem.category.rawValue)
                            .font(.caption.weight(.medium))
                            .foregroundColor(menuItem.category.color)

                        Text(menuItem.getTaxBandName(using: posData.taxBands))
                            .font(.caption.weight(.medium))
                            .foregroundColor(POSColors.info)
                    }
                }

                Spacer()

                VStack(alignment: .trailing, spacing: 3) {
                    Text(menuItem.price.formatted(.currency(code: currencyCode)))
                        .font(.title3.weight(.bold))
                        .foregroundColor(POSColors.goldPrimary)

                    Text("Cost: \(menuItem.unitCost.formatted(.currency(code: currencyCode)))")
                        .font(.caption)
                        .foregroundColor(POSColors.textSecondary)
                }
            }

            // 86'd toggle
            HStack {
                Label(menuItem.isAvailable ? "Available" : "86'd — Unavailable", systemImage: menuItem.isAvailable ? "checkmark.circle.fill" : "nosign")
                    .font(.subheadline.weight(.medium))
                    .foregroundColor(menuItem.isAvailable ? POSColors.success : POSColors.warning)

                Spacer()

                Toggle("", isOn: Binding(
                    get: { menuItem.isAvailable },
                    set: { newValue in
                        if let index = posData.menuItems.firstIndex(where: { $0.id == menuItem.id }) {
                            posData.menuItems[index].isAvailable = newValue
                            posData.saveMenuItems()
                        }
                    }
                ))
                .tint(POSColors.success)
                .labelsHidden()
            }

            if !menuItem.availableModifiers.isEmpty {
                HStack {
                    Text("\(menuItem.availableModifiers.count) modifier\(menuItem.availableModifiers.count == 1 ? "" : "s")")
                        .font(.caption)
                        .foregroundColor(POSColors.textMuted)
                    Spacer()
                }
            }

            HStack(spacing: 8) {
                Button("Modifiers") {
                    selectedMenuItem = menuItem
                }
                .buttonStyle(SecondaryButtonStyle())

                Button("Remove") {
                    removeMenuItem(menuItem)
                }
                .buttonStyle(PrimaryButtonStyle(isDestructive: true))
            }
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: POSRadius.medium)
                .fill(menuItem.isAvailable ? POSColors.backgroundMedium : POSColors.backgroundMedium.opacity(0.6))
                .shadow(color: POSShadows.card.color, radius: POSShadows.card.radius, x: POSShadows.card.x, y: POSShadows.card.y)
        )
        .overlay(
            RoundedRectangle(cornerRadius: POSRadius.medium)
                .stroke(menuItem.isAvailable ? Color.clear : POSColors.warning.opacity(0.4), lineWidth: 1)
        )
    }
    
    private var addItemSheet: some View {
        NavigationView {
            ScrollView {
                VStack(spacing: 24) {
                    Text("Add New Menu Item")
                        .font(.title.weight(.bold))
                        .foregroundColor(POSColors.textPrimary)
                    
                    VStack(alignment: .leading, spacing: 8) {
                        Text("Item Name")
                            .font(.headline.weight(.medium))
                            .foregroundColor(POSColors.textPrimary)
                        
                        TextField("Item name", text: $newItemName)
                            .textFieldStyle(RoundedBorderTextFieldStyle())
                    }
                    
                    HStack(spacing: 16) {
                        VStack(alignment: .leading, spacing: 8) {
                            Text("Price")
                                .font(.headline.weight(.medium))
                                .foregroundColor(POSColors.textPrimary)
                            
                            TextField("0.00", text: $newItemPrice)
                                .textFieldStyle(RoundedBorderTextFieldStyle())
                                .keyboardType(.decimalPad)
                        }
                        
                        VStack(alignment: .leading, spacing: 8) {
                            Text("Unit Cost")
                                .font(.headline.weight(.medium))
                                .foregroundColor(POSColors.textPrimary)
                            
                            TextField("0.00", text: $newItemCost)
                                .textFieldStyle(RoundedBorderTextFieldStyle())
                                .keyboardType(.decimalPad)
                        }
                    }
                    
                    VStack(alignment: .leading, spacing: 8) {
                        Text("Category")
                            .font(.headline.weight(.medium))
                            .foregroundColor(POSColors.textPrimary)

                        Picker("Category", selection: $newItemCategory) {
                            ForEach(MenuCategory.allCases) { category in
                                Label(category.rawValue, systemImage: category.icon)
                                    .tag(category)
                            }
                        }
                        .pickerStyle(MenuPickerStyle())
                    }

                    VStack(alignment: .leading, spacing: 8) {
                        Text("Tax Band")
                            .font(.headline.weight(.medium))
                            .foregroundColor(POSColors.textPrimary)

                        Picker("Tax Band", selection: $newItemTaxBandId) {
                            Text("None").tag(nil as UUID?)
                            ForEach(posData.taxBands) { band in
                                Text("\(band.name) (\(band.displayRate))").tag(band.id as UUID?)
                            }
                        }
                        .pickerStyle(MenuPickerStyle())
                    }
                    
                    
                    Spacer()
                    
                    HStack(spacing: 16) {
                        Button("Cancel") {
                            resetForm()
                            showingAddItem = false
                        }
                        .buttonStyle(SecondaryButtonStyle())
                        
                        Button("Add Item") {
                            addMenuItem()
                        }
                        .buttonStyle(PrimaryButtonStyle())
                        .disabled(newItemName.isEmpty || newItemPrice.isEmpty || newItemCost.isEmpty)
                    }
                }
                .padding(20)
            }
            .background(POSColors.backgroundGradient)
            .navigationBarHidden(true)
        }
        .preferredColorScheme(.dark)
    }
    
    
    private func addMenuItem() {
        guard !newItemName.isEmpty,
              let price = Double(newItemPrice),
              let cost = Double(newItemCost),
              price > 0, cost >= 0 else { return }
        
        let menuItem = MenuItem(
            name: newItemName,
            price: price,
            taxCategory: .nonAlcoholic,
            unitCost: cost,
            taxBandId: newItemTaxBandId,
            availableModifiers: [],
            category: newItemCategory
        )
        
        posData.menuItems.append(menuItem)
        posData.saveMenuItems()
        
        resetForm()
        showingAddItem = false
    }
    
    private func removeMenuItem(_ menuItem: MenuItem) {
        posData.menuItems.removeAll { $0.id == menuItem.id }
        posData.saveMenuItems()
    }
    
    private func resetForm() {
        newItemName = ""
        newItemPrice = ""
        newItemCost = ""
        newItemTaxBandId = nil
        newItemCategory = .other
    }
}

struct MenuSettingsView_Previews: PreviewProvider {
    static var previews: some View {
        NavigationView {
            MenuSettingsView()
                .environmentObject(POSData())
        }
        .preferredColorScheme(.dark)
    }
}
