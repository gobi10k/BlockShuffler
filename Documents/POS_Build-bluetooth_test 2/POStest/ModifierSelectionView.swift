import SwiftUI

struct ModifierSelectionView: View {
    let menuItem: MenuItem
    let onComplete: ([MenuItemModifier]) -> Void

    @AppStorage("currencyCode") private var currencyCode: String = "EUR"
    @State private var selectedModifiers: Set<MenuItemModifier> = []
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                // Header card
                VStack(spacing: 6) {
                    Image(systemName: menuItem.category.icon)
                        .font(.title)
                        .foregroundColor(menuItem.category.color)

                    Text(menuItem.name)
                        .font(.title2.weight(.bold))
                        .foregroundColor(POSColors.textPrimary)

                    Text(menuItem.price, format: .currency(code: currencyCode))
                        .font(.headline.weight(.semibold))
                        .foregroundColor(POSColors.goldPrimary)
                }
                .frame(maxWidth: .infinity)
                .padding(.vertical, 20)
                .padding(.horizontal, 16)
                .background(POSColors.backgroundMedium)

                // Modifier list
                ScrollView {
                    LazyVStack(spacing: 10) {
                        ForEach(menuItem.availableModifiers) { modifier in
                            modifierRow(modifier)
                        }
                    }
                    .padding(16)
                }

                // Add to order button
                Button(action: {
                    onComplete(Array(selectedModifiers))
                    dismiss()
                }) {
                    HStack {
                        Text("Add to Order")
                        if !selectedModifiers.isEmpty {
                            let extra = selectedModifiers.reduce(0.0) { $0 + $1.priceAdjustment }
                            if extra != 0 {
                                Text("+\(extra, format: .currency(code: currencyCode))")
                                    .font(.subheadline.weight(.semibold))
                            }
                        }
                    }
                }
                .buttonStyle(PrimaryButtonStyle())
                .padding(16)
            }
            .background(POSColors.backgroundGradient)
            .navigationTitle("Customise")
            .navigationBarTitleDisplayMode(.inline)
            .toolbarBackground(POSColors.backgroundDark, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbarColorScheme(.dark, for: .navigationBar)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { dismiss() }
                        .foregroundColor(POSColors.goldPrimary)
                }
            }
        }
        .preferredColorScheme(.dark)
    }

    private func modifierRow(_ modifier: MenuItemModifier) -> some View {
        let isSelected = selectedModifiers.contains(modifier)

        return Button(action: { toggleSelection(for: modifier) }) {
            HStack(spacing: 14) {
                Image(systemName: isSelected ? "checkmark.circle.fill" : "circle")
                    .font(.title3)
                    .foregroundColor(isSelected ? POSColors.goldPrimary : POSColors.textMuted)

                Text(modifier.name)
                    .font(.headline.weight(.medium))
                    .foregroundColor(POSColors.textPrimary)

                Spacer()

                Text(modifier.displayAdjustment)
                    .font(.subheadline.weight(.semibold))
                    .foregroundColor(modifier.priceAdjustment >= 0 ? POSColors.goldPrimary : POSColors.success)
            }
            .padding(.vertical, 14)
            .padding(.horizontal, 16)
            .background(
                RoundedRectangle(cornerRadius: POSRadius.medium)
                    .fill(isSelected ? POSColors.goldPrimary.opacity(0.1) : POSColors.backgroundMedium)
                    .overlay(
                        RoundedRectangle(cornerRadius: POSRadius.medium)
                            .stroke(isSelected ? POSColors.goldPrimary.opacity(0.5) : Color.clear, lineWidth: 1)
                    )
            )
        }
        .buttonStyle(.plain)
        .animation(.easeInOut(duration: 0.15), value: isSelected)
    }

    private func toggleSelection(for modifier: MenuItemModifier) {
        if selectedModifiers.contains(modifier) {
            selectedModifiers.remove(modifier)
        } else {
            selectedModifiers.insert(modifier)
        }
    }
}
