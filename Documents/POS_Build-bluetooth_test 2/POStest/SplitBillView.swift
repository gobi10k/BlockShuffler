import SwiftUI

// MARK: - SplitBillView

struct SplitBillView: View {

    // MARK: - Types

    enum SplitMode: String, CaseIterable {
        case equal  = "Equal"
        case byItem = "By Item"
        case custom = "Custom"

        var icon: String {
            switch self {
            case .equal:  return "divide.circle"
            case .byItem: return "list.bullet.rectangle.portrait"
            case .custom: return "slider.horizontal.3"
            }
        }
    }

    /// A single payable portion of the bill.
    struct Portion: Identifiable {
        let id = UUID()
        var label: String
        var amount: Double
        var isPaid: Bool = false
        var paymentMethod: String? = nil
    }

    // MARK: - Properties

    @EnvironmentObject var posData: POSData
    @Environment(\.dismiss) private var dismiss

    let table: Table
    let tipAmount: Double
    /// Called when every portion is settled — passes combined payment summary string.
    let onComplete: (String) -> Void

    @AppStorage("currencyCode") private var currencyCode: String = "EUR"

    // Mode
    @State private var splitMode: SplitMode = .equal

    // Equal split
    @State private var numberOfWays: Int = 2
    @State private var equalPaid: [Bool] = [false, false]
    @State private var equalMethods: [String?] = [nil, nil]

    // By-item split
    @State private var byItemPeople: Int = 2
    @State private var itemAssignments: [UUID: Int] = [:]    // orderItem.id → 0-based person index
    @State private var byItemPaid: [Bool] = [false, false]
    @State private var byItemMethods: [String?] = [nil, nil]

    // Custom split
    @State private var customStrings: [String] = ["", ""]
    @State private var customPaid: [Bool] = [false, false]
    @State private var customMethods: [String?] = [nil, nil]

    // Payment confirmation
    @State private var pendingIndex: Int? = nil
    @State private var showPaymentSheet = false

    // MARK: - Derived

    private var grandTotal: Double { table.total + tipAmount }

    // MARK: - Body

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                modePicker
                ScrollView {
                    VStack(spacing: 0) {
                        totalsBar
                        Group {
                            switch splitMode {
                            case .equal:  equalSection
                            case .byItem: byItemSection
                            case .custom: customSection
                            }
                        }
                        .padding(16)
                    }
                }
            }
            .background(POSColors.backgroundGradient)
            .navigationTitle("Split Bill — Table \(table.id)")
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
            .confirmationDialog(
                "How would you like to pay?",
                isPresented: $showPaymentSheet,
                titleVisibility: .visible
            ) {
                Button("Cash") { payPortion(method: "Cash") }
                Button("Card (SumUp)") { payPortion(method: "SumUp") }
                Button("Cancel", role: .cancel) { pendingIndex = nil }
            }
        }
        .preferredColorScheme(.dark)
        .onChange(of: splitMode) { resetPaid() }
        .onChange(of: numberOfWays) { resizeEqual() }
        .onChange(of: byItemPeople) { resizeByItem() }
        .onChange(of: customStrings.count) { }
    }

    // MARK: - Mode Picker

    private var modePicker: some View {
        HStack(spacing: 0) {
            ForEach(SplitMode.allCases, id: \.self) { mode in
                Button {
                    withAnimation(.easeInOut(duration: 0.2)) { splitMode = mode }
                } label: {
                    VStack(spacing: 4) {
                        Image(systemName: mode.icon)
                            .font(.subheadline)
                        Text(mode.rawValue)
                            .font(.caption.weight(.semibold))
                    }
                    .foregroundColor(splitMode == mode ? POSColors.backgroundDark : POSColors.goldPrimary)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 10)
                    .background(splitMode == mode ? POSColors.goldPrimary : Color.clear)
                }
            }
        }
        .background(POSColors.backgroundMedium)
    }

    // MARK: - Totals bar

    private var totalsBar: some View {
        HStack(spacing: 0) {
            totalCell(label: "Order", amount: table.total)
            Divider().frame(height: 40).background(POSColors.goldSubtle.opacity(0.3))
            totalCell(label: "Tip", amount: tipAmount)
            Divider().frame(height: 40).background(POSColors.goldSubtle.opacity(0.3))
            totalCell(label: "Total", amount: grandTotal, highlight: true)
        }
        .padding(.vertical, 12)
        .background(POSColors.backgroundDark)
    }

    private func totalCell(label: String, amount: Double, highlight: Bool = false) -> some View {
        VStack(spacing: 2) {
            Text(label)
                .font(.caption.weight(.medium))
                .foregroundColor(POSColors.textMuted)
            Text(amount, format: .currency(code: currencyCode))
                .font(.subheadline.weight(.bold))
                .foregroundColor(highlight ? POSColors.goldPrimary : POSColors.textPrimary)
        }
        .frame(maxWidth: .infinity)
    }

    // MARK: - Equal Split

    private var equalSection: some View {
        VStack(spacing: 16) {
            // Ways stepper
            HStack {
                Text("Split \(numberOfWays) ways")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                Spacer()
                Stepper("", value: $numberOfWays, in: 2...20)
                    .labelsHidden()
            }
            .padding(16)
            .background(RoundedRectangle(cornerRadius: POSRadius.medium).fill(POSColors.backgroundMedium))

            // Portion cards
            let portionAmount = grandTotal / Double(numberOfWays)
            ForEach(0..<numberOfWays, id: \.self) { i in
                portionCard(
                    label: "Person \(i + 1)",
                    amount: portionAmount,
                    isPaid: safeGet(equalPaid, i),
                    method: safeGet(equalMethods, i) ?? nil,
                    index: i
                )
            }

            progressFooter(paid: equalPaid.filter { $0 }.count, total: numberOfWays)
        }
    }

    // MARK: - By Item Split

    private var byItemSection: some View {
        VStack(spacing: 16) {
            // People stepper
            HStack {
                Text("\(byItemPeople) people")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                Spacer()
                Stepper("", value: $byItemPeople, in: 2...20)
                    .labelsHidden()
            }
            .padding(16)
            .background(RoundedRectangle(cornerRadius: POSRadius.medium).fill(POSColors.backgroundMedium))

            // Item assignment list
            VStack(spacing: 2) {
                HStack {
                    Text("Assign items")
                        .font(.subheadline.weight(.semibold))
                        .foregroundColor(POSColors.textPrimary)
                    Spacer()
                    Text("Tap item to reassign")
                        .font(.caption)
                        .foregroundColor(POSColors.textMuted)
                }
                .padding(.bottom, 6)

                ForEach(table.items) { item in
                    let assignedTo = itemAssignments[item.id] ?? 0
                    Button {
                        // Cycle to next person
                        itemAssignments[item.id] = (assignedTo + 1) % byItemPeople
                    } label: {
                        HStack(spacing: 10) {
                            Circle()
                                .fill(personColor(assignedTo))
                                .frame(width: 28, height: 28)
                                .overlay(
                                    Text("\(assignedTo + 1)")
                                        .font(.caption.weight(.bold))
                                        .foregroundColor(.white)
                                )

                            VStack(alignment: .leading, spacing: 2) {
                                Text(item.menuItem.name)
                                    .font(.subheadline.weight(.medium))
                                    .foregroundColor(POSColors.textPrimary)
                                if !item.appliedModifiers.isEmpty {
                                    Text(item.appliedModifiers.map { $0.name }.joined(separator: ", "))
                                        .font(.caption)
                                        .foregroundColor(POSColors.goldPrimary)
                                }
                            }

                            Spacer()

                            Text(item.finalPrice, format: .currency(code: currencyCode))
                                .font(.subheadline.weight(.semibold))
                                .foregroundColor(POSColors.textSecondary)
                        }
                        .padding(.vertical, 10)
                        .padding(.horizontal, 14)
                        .background(
                            RoundedRectangle(cornerRadius: POSRadius.medium)
                                .fill(personColor(assignedTo).opacity(0.08))
                                .overlay(
                                    RoundedRectangle(cornerRadius: POSRadius.medium)
                                        .stroke(personColor(assignedTo).opacity(0.3), lineWidth: 1)
                                )
                        )
                    }
                    .buttonStyle(.plain)
                }
            }
            .padding(16)
            .background(RoundedRectangle(cornerRadius: POSRadius.medium).fill(POSColors.backgroundMedium))

            // Per-person totals
            let tipPerPortion = grandTotal > 0 ? tipAmount / grandTotal : 0

            ForEach(0..<byItemPeople, id: \.self) { i in
                let assigned = table.items.filter { (itemAssignments[$0.id] ?? 0) == i }
                let subtotal = assigned.reduce(0) { $0 + $1.finalPrice }
                let total = subtotal * (1 + tipPerPortion) // proportional tip share
                portionCard(
                    label: "Person \(i + 1)",
                    amount: total,
                    isPaid: safeGet(byItemPaid, i),
                    method: safeGet(byItemMethods, i) ?? nil,
                    index: i,
                    detail: "\(assigned.count) item\(assigned.count == 1 ? "" : "s")"
                )
            }

            progressFooter(paid: byItemPaid.filter { $0 }.count, total: byItemPeople)
        }
    }

    // MARK: - Custom Split

    private var customSection: some View {
        VStack(spacing: 16) {
            // People management
            HStack {
                Text("\(customStrings.count) people")
                    .font(.headline.weight(.semibold))
                    .foregroundColor(POSColors.textPrimary)
                Spacer()
                HStack(spacing: 12) {
                    Button {
                        guard customStrings.count > 2 else { return }
                        customStrings.removeLast()
                        customPaid.removeLast()
                        customMethods.removeLast()
                    } label: {
                        Image(systemName: "minus.circle.fill")
                            .font(.title3)
                            .foregroundColor(customStrings.count > 2 ? POSColors.error : POSColors.textDisabled)
                    }
                    .disabled(customStrings.count <= 2)

                    Button {
                        customStrings.append("")
                        customPaid.append(false)
                        customMethods.append(nil)
                    } label: {
                        Image(systemName: "plus.circle.fill")
                            .font(.title3)
                            .foregroundColor(POSColors.goldPrimary)
                    }
                }
            }
            .padding(16)
            .background(RoundedRectangle(cornerRadius: POSRadius.medium).fill(POSColors.backgroundMedium))

            // Amount inputs
            VStack(spacing: 10) {
                ForEach(0..<customStrings.count, id: \.self) { i in
                    HStack(spacing: 12) {
                        Circle()
                            .fill(personColor(i))
                            .frame(width: 28, height: 28)
                            .overlay(
                                Text("\(i + 1)")
                                    .font(.caption.weight(.bold))
                                    .foregroundColor(.white)
                            )

                        Text("Person \(i + 1)")
                            .font(.subheadline.weight(.medium))
                            .foregroundColor(POSColors.textPrimary)

                        Spacer()

                        TextField("0.00", text: Binding(
                            get: { customStrings[i] },
                            set: { customStrings[i] = $0 }
                        ))
                        .font(.headline.weight(.semibold))
                        .foregroundColor(POSColors.goldPrimary)
                        .multilineTextAlignment(.trailing)
                        .keyboardType(.decimalPad)
                        .frame(width: 90)
                    }
                    .padding(.vertical, 10)
                    .padding(.horizontal, 14)
                    .background(RoundedRectangle(cornerRadius: POSRadius.medium).fill(POSColors.backgroundMedium))
                }
            }

            // Balance indicator
            let entered = customStrings.compactMap { Double($0) }.reduce(0, +)
            let remainder = grandTotal - entered
            HStack {
                Text(abs(remainder) < 0.005 ? "Balanced" : (remainder > 0 ? "Remaining:" : "Over by:"))
                    .font(.subheadline.weight(.medium))
                    .foregroundColor(abs(remainder) < 0.005 ? POSColors.success : POSColors.warning)
                Spacer()
                if abs(remainder) >= 0.005 {
                    Text(abs(remainder), format: .currency(code: currencyCode))
                        .font(.subheadline.weight(.bold))
                        .foregroundColor(remainder > 0 ? POSColors.warning : POSColors.error)
                } else {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundColor(POSColors.success)
                }
            }
            .padding(14)
            .background(RoundedRectangle(cornerRadius: POSRadius.medium)
                .fill(abs(remainder) < 0.005 ? POSColors.success.opacity(0.1) : POSColors.backgroundMedium))

            // Pay buttons — only enabled when balanced
            let isBalanced = abs(remainder) < 0.005
            ForEach(0..<customStrings.count, id: \.self) { i in
                let amount = Double(customStrings[i]) ?? 0
                portionCard(
                    label: "Person \(i + 1)",
                    amount: amount,
                    isPaid: safeGet(customPaid, i),
                    method: safeGet(customMethods, i) ?? nil,
                    index: i,
                    disabled: !isBalanced || amount <= 0
                )
            }

            progressFooter(paid: customPaid.filter { $0 }.count, total: customStrings.count)
        }
    }

    // MARK: - Shared Portion Card

    private func portionCard(
        label: String,
        amount: Double,
        isPaid: Bool,
        method: String?,
        index: Int,
        detail: String? = nil,
        disabled: Bool = false
    ) -> some View {
        HStack(spacing: 14) {
            // Person badge
            ZStack {
                Circle()
                    .fill(isPaid ? POSColors.success : personColor(index))
                    .frame(width: 40, height: 40)
                if isPaid {
                    Image(systemName: "checkmark")
                        .font(.subheadline.weight(.bold))
                        .foregroundColor(.white)
                } else {
                    Text("\(index + 1)")
                        .font(.subheadline.weight(.bold))
                        .foregroundColor(.white)
                }
            }

            VStack(alignment: .leading, spacing: 3) {
                Text(label)
                    .font(.headline.weight(.semibold))
                    .foregroundColor(isPaid ? POSColors.textMuted : POSColors.textPrimary)
                if let detail {
                    Text(detail)
                        .font(.caption)
                        .foregroundColor(POSColors.textMuted)
                }
                if isPaid, let method {
                    Label(method, systemImage: method == "Cash" ? "banknote" : "creditcard")
                        .font(.caption.weight(.medium))
                        .foregroundColor(POSColors.success)
                }
            }

            Spacer()

            if isPaid {
                Text(amount, format: .currency(code: currencyCode))
                    .font(.title3.weight(.bold))
                    .foregroundColor(POSColors.success)
                    .strikethrough(false)
            } else {
                VStack(alignment: .trailing, spacing: 6) {
                    Text(amount, format: .currency(code: currencyCode))
                        .font(.title3.weight(.bold))
                        .foregroundColor(disabled ? POSColors.textDisabled : POSColors.goldPrimary)

                    if !disabled {
                        Button("Pay") {
                            pendingIndex = index
                            showPaymentSheet = true
                        }
                        .font(.subheadline.weight(.semibold))
                        .foregroundColor(POSColors.backgroundDark)
                        .padding(.horizontal, 16)
                        .padding(.vertical, 7)
                        .background(Capsule().fill(POSColors.goldPrimary))
                    }
                }
            }
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: POSRadius.medium)
                .fill(isPaid ? POSColors.success.opacity(0.06) : POSColors.backgroundMedium)
                .overlay(
                    RoundedRectangle(cornerRadius: POSRadius.medium)
                        .stroke(isPaid ? POSColors.success.opacity(0.3) : Color.clear, lineWidth: 1)
                )
        )
        .opacity(disabled ? 0.5 : 1.0)
    }

    // MARK: - Progress Footer

    private func progressFooter(paid: Int, total: Int) -> some View {
        HStack(spacing: 12) {
            // Progress bar
            GeometryReader { geo in
                ZStack(alignment: .leading) {
                    RoundedRectangle(cornerRadius: 4)
                        .fill(POSColors.backgroundLight.opacity(0.3))
                    RoundedRectangle(cornerRadius: 4)
                        .fill(paid == total ? POSColors.success : POSColors.goldPrimary)
                        .frame(width: geo.size.width * (total > 0 ? CGFloat(paid) / CGFloat(total) : 0))
                        .animation(.easeInOut(duration: 0.3), value: paid)
                }
            }
            .frame(height: 8)

            Text("\(paid) / \(total) paid")
                .font(.subheadline.weight(.semibold))
                .foregroundColor(paid == total ? POSColors.success : POSColors.textSecondary)
                .frame(width: 80, alignment: .trailing)
        }
        .padding(.top, 4)
    }

    // MARK: - Payment Action

    private func payPortion(method: String) {
        guard let i = pendingIndex else { return }

        let amount: Double
        switch splitMode {
        case .equal:
            amount = grandTotal / Double(numberOfWays)
        case .byItem:
            let assigned = table.items.filter { (itemAssignments[$0.id] ?? 0) == i }
            let subtotal = assigned.reduce(0) { $0 + $1.finalPrice }
            let tipPerPortion = grandTotal > 0 ? tipAmount / grandTotal : 0
            amount = subtotal * (1 + tipPerPortion)
        case .custom:
            amount = Double(customStrings[i]) ?? 0
        }

        if method == "SumUp" {
            SumUpAPIManager.shared.processPayment(amount: amount, currency: currencyCode) { [i] success in
                if success {
                    DispatchQueue.main.async { self.markPaid(index: i, method: method) }
                }
            }
        } else {
            markPaid(index: i, method: method)
        }

        pendingIndex = nil
    }

    private func markPaid(index i: Int, method: String) {
        switch splitMode {
        case .equal:
            guard i < equalPaid.count else { return }
            equalPaid[i] = true
            equalMethods[i] = method
            if equalPaid.allSatisfy({ $0 }) { finalize() }

        case .byItem:
            guard i < byItemPaid.count else { return }
            byItemPaid[i] = true
            byItemMethods[i] = method
            if byItemPaid.allSatisfy({ $0 }) { finalize() }

        case .custom:
            guard i < customPaid.count else { return }
            customPaid[i] = true
            customMethods[i] = method
            if customPaid.allSatisfy({ $0 }) { finalize() }
        }
    }

    private func finalize() {
        // Summarise which payment methods were used
        let methods: [String]
        switch splitMode {
        case .equal:  methods = equalMethods.compactMap { $0 }
        case .byItem: methods = byItemMethods.compactMap { $0 }
        case .custom: methods = customMethods.compactMap { $0 }
        }
        let unique = Array(Set(methods)).sorted()
        let summary = unique.isEmpty ? "Split" : "Split (\(unique.joined(separator: " & ")))"
        onComplete(summary)
        dismiss()
    }

    // MARK: - Helpers

    private func resetPaid() {
        let n: Int
        switch splitMode {
        case .equal:  n = numberOfWays
        case .byItem: n = byItemPeople
        case .custom: n = customStrings.count
        }
        equalPaid      = Array(repeating: false, count: n)
        equalMethods   = Array(repeating: nil, count: n)
        byItemPaid     = Array(repeating: false, count: n)
        byItemMethods  = Array(repeating: nil, count: n)
        customPaid     = Array(repeating: false, count: n)
        customMethods  = Array(repeating: nil, count: n)
    }

    private func resizeEqual() {
        equalPaid    = Array(repeating: false, count: numberOfWays)
        equalMethods = Array(repeating: nil,   count: numberOfWays)
    }

    private func resizeByItem() {
        byItemPaid    = Array(repeating: false, count: byItemPeople)
        byItemMethods = Array(repeating: nil,   count: byItemPeople)
        // Clamp existing assignments
        for key in itemAssignments.keys where (itemAssignments[key] ?? 0) >= byItemPeople {
            itemAssignments[key] = byItemPeople - 1
        }
    }

    /// Safe array access returning nil for out-of-range.
    private func safeGet<T>(_ array: [T], _ index: Int) -> T? {
        guard index >= 0 && index < array.count else { return nil }
        return array[index]
    }

    private let personColors: [Color] = [
        Color(hex: "#60A5FA"),   // blue
        Color(hex: "#F472B6"),   // pink
        Color(hex: "#34D399"),   // green
        Color(hex: "#FBBF24"),   // amber
        Color(hex: "#A78BFA"),   // purple
        Color(hex: "#FB7185"),   // rose
        Color(hex: "#2DD4BF"),   // teal
        Color(hex: "#F97316"),   // orange
    ]

    private func personColor(_ index: Int) -> Color {
        personColors[index % personColors.count]
    }
}
