//
//  OpenTableView.swift
//  POStest
//
//  Created by Jules on 11/08/2025.
//

import SwiftUI

struct OpenTableView: View {
    @Binding var guestCount: Int?
    @Binding var groupType: GroupType?

    let onConfirm: () -> Void
    let onCancel: () -> Void

    let guestOptions = Array(1...10)
    let groupTypeOptions = GroupType.allCases

    var body: some View {
        NavigationView {
            Form {
                Section(header: Text("Select Guest Count")) {
                    ScrollView(.horizontal, showsIndicators: false) {
                        HStack {
                            ForEach(guestOptions, id: \.self) { count in
                                Button(action: {
                                    self.guestCount = count
                                }) {
                                    Text("\(count)")
                                        .font(.title)
                                        .frame(width: 60, height: 60)
                                        .background(self.guestCount == count ? Color.blue : Color.gray.opacity(0.2))
                                        .foregroundColor(self.guestCount == count ? .white : .primary)
                                        .cornerRadius(10)
                                }
                            }
                        }
                    }
                    .padding(.vertical)
                }

                Section(header: Text("Select Group Type")) {
                    Picker("Group Type", selection: $groupType) {
                        Text("None").tag(nil as GroupType?)
                        ForEach(groupTypeOptions) { type in
                            Text(type.rawValue).tag(type as GroupType?)
                        }
                    }
                    .pickerStyle(SegmentedPickerStyle())
                }
            }
            .navigationTitle("Open Table")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel", action: onCancel)
                }
                ToolbarItem(placement: .confirmationAction) {
                    Button("Confirm", action: onConfirm)
                        .disabled(guestCount == nil || groupType == nil)
                }
            }
        }
    }
}

struct OpenTableView_Previews: PreviewProvider {
    static var previews: some View {
        OpenTableView(
            guestCount: .constant(2),
            groupType: .constant(.couple),
            onConfirm: {},
            onCancel: {}
        )
    }
}
