//
//  SumUpAPIManager.swift
//  POStest
//
//  Created by Jules on 12/08/2025.
//

import Foundation
import SumUpSDK

class SumUpAPIManager {
    static let shared = SumUpAPIManager()

    private init() {}

    func processPayment(amount: Double, currency: String, completion: @escaping (Bool) -> Void) {
        guard let windowScene = UIApplication.shared.connectedScenes.first as? UIWindowScene,
              let rootViewController = windowScene.windows.first?.rootViewController else {
            completion(false)
            return
        }

        let request = CheckoutRequest(total: NSDecimalNumber(value: amount),
                                      title: "POS Test Payment",
                                      currencyCode: currency)

        SumUpSDK.checkout(with: request, from: rootViewController) { (result, error) in
            if let error = error {
                print("Error processing payment: \(error.localizedDescription)")
                completion(false)
                return
            }

            if let result = result, result.success {
                print("Payment successful!")
                completion(true)
            } else {
                print("Payment failed.")
                completion(false)
            }
        }
    }
}
