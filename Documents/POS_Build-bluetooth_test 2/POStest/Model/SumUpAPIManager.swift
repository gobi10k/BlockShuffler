//
//  SumUpAPIManager.swift
//  POStest
//

import UIKit
import SumUpSDK

class SumUpAPIManager {
    static let shared = SumUpAPIManager()

    private init() {}

    var isLoggedIn: Bool {
        SumUpSDK.isLoggedIn
    }

    /// Re-initialise the SDK with a new API key (call when the key changes in Settings).
    func setup(withAPIKey key: String) {
        SumUpSDK.setup(withAPIKey: key)
    }

    func processPayment(
        amount: Double,
        currency: String,
        title: String = "Five Points",
        completion: @escaping (Bool) -> Void
    ) {
        guard let rootVC = keyRootViewController() else {
            completion(false)
            return
        }

        let request = CheckoutRequest(
            total: NSDecimalNumber(value: amount),
            title: title,
            currencyCode: currency
        )

        SumUpSDK.checkout(with: request, from: rootVC) { result, error in
            if let error = error {
                print("[SumUp] Payment error: \(error.localizedDescription)")
                completion(false)
                return
            }
            let success = result?.success ?? false
            if !success { print("[SumUp] Payment not successful.") }
            completion(success)
        }
    }

    func presentLogin(completion: ((Bool) -> Void)? = nil) {
        guard let rootVC = keyRootViewController() else {
            completion?(false)
            return
        }
        SumUpSDK.presentLogin(from: rootVC, animated: true) { success, error in
            if let error = error {
                print("[SumUp] Login error: \(error.localizedDescription)")
            }
            completion?(success)
        }
    }

    // MARK: - Private

    private func keyRootViewController() -> UIViewController? {
        UIApplication.shared.connectedScenes
            .compactMap { $0 as? UIWindowScene }
            .flatMap { $0.windows }
            .first { $0.isKeyWindow }?
            .rootViewController
    }
}
