import SwiftUI
import WebKit

@main
struct PandaWebApp: App {
    @State private var host = Host()
    var body: some Scene {
        WindowGroup {
            WebRep(host: host).ignoresSafeArea().statusBarHidden().onAppear { host.start() }
        }
    }
}

struct WebRep: UIViewRepresentable {
    let host: Host
    func makeUIView(context: Context) -> WKWebView { host.webView }
    func updateUIView(_ uiView: WKWebView, context: Context) {}
}

func plog(_ s: String) {
    print("PANDA|" + s)
    fflush(stdout)
}

// Serves the bundled Site/ folder under panda://app/ (any path maps to Site/<path>)
final class SiteHandler: NSObject, WKURLSchemeHandler {
    let root = Bundle.main.url(forResource: "Site", withExtension: nil)!
    func webView(_ webView: WKWebView, start task: WKURLSchemeTask) {
        let url = task.request.url!
        let rel = String(url.path.dropFirst())
        guard let data = try? Data(contentsOf: root.appendingPathComponent(rel)) else {
            task.didReceive(HTTPURLResponse(url: url, statusCode: 404, httpVersion: "HTTP/1.1", headerFields: nil)!)
            task.didFinish(); return
        }
        let type: String
        if rel.hasSuffix(".html") { type = "text/html; charset=utf-8" }
        else if rel.hasSuffix(".js") { type = "text/javascript; charset=utf-8" }
        else if rel.hasSuffix(".wasm") { type = "application/wasm" }
        else { type = "application/octet-stream" }
        let headers = ["Content-Type": type, "Content-Length": "\(data.count)", "Cache-Control": "no-store"]
        task.didReceive(HTTPURLResponse(url: url, statusCode: 200, httpVersion: "HTTP/1.1", headerFields: headers)!)
        task.didReceive(data)
        task.didFinish()
    }
    func webView(_ webView: WKWebView, stop task: WKURLSchemeTask) {}
}

final class Host: NSObject, WKScriptMessageHandler, WKNavigationDelegate {
    let webView: WKWebView
    var timer: Timer?

    override init() {
        let cfg = WKWebViewConfiguration()
        cfg.setURLSchemeHandler(SiteHandler(), forURLScheme: "panda")
        let uc = WKUserContentController()
        cfg.userContentController = uc
        webView = WKWebView(frame: .zero, configuration: cfg)
        super.init()
        uc.add(self, name: "panda")
        webView.navigationDelegate = self
        webView.isInspectable = true
        webView.scrollView.isScrollEnabled = false
        webView.scrollView.bounces = false
        webView.backgroundColor = .black
        webView.isOpaque = true
    }

    func start() {
        UIApplication.shared.isIdleTimerDisabled = true
        let d = UserDefaults.standard
        let rom = d.string(forKey: "rom") ?? "/run/ctr-hello.3dsx"
        let frames = d.string(forKey: "frames") ?? "0"
        let turbo = d.string(forKey: "turbo") ?? "0"
        let name = d.string(forKey: "name") ?? "run"
        let report = d.string(forKey: "report") ?? "1"
        var info = utsname(); uname(&info)
        let model = withUnsafePointer(to: &info.machine) { $0.withMemoryRebound(to: CChar.self, capacity: 64) { String(cString: $0) } }
        plog("{\"type\":\"launch\",\"name\":\"\(name)\",\"ios\":\"\(UIDevice.current.systemVersion)\",\"model\":\"\(model)\",\"rom\":\"\(rom)\",\"frames\":\(frames),\"turbo\":\(turbo)}")
        let dbg = d.string(forKey: "dbg") ?? "0"
        let env = d.string(forKey: "env") ?? ""
        let q = "rom=\(rom)&frames=\(frames)&turbo=\(turbo)&name=\(name)&report=\(report)&dbg=\(dbg)&env=\(env)"
        let page = d.string(forKey: "page") ?? "index.html"
        let raw = d.string(forKey: "q") ?? q   // `-q "n=8192&mode=inst"` replaces the generated query (for stress pages)
        webView.load(URLRequest(url: URL(string: "panda://app/web/\(page)?\(raw)")!))
        timer = Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { _ in
            plog("{\"type\":\"native\",\"thermal\":\(ProcessInfo.processInfo.thermalState.rawValue),\"appAvailMB\":\(os_proc_available_memory() / 1_048_576),\"lowPower\":\(ProcessInfo.processInfo.isLowPowerModeEnabled)}")
        }
    }

    func userContentController(_ c: WKUserContentController, didReceive m: WKScriptMessage) {
        guard let s = m.body as? String else { return }
        plog(s)
        if s.contains("\"type\":\"done\"") { DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { exit(0) } }
    }
    func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
        plog("{\"type\":\"webcontent_terminated\"}")
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { exit(2) }
    }
    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) { plog("{\"type\":\"nav_fail\",\"e\":\"\(error)\"}") }
    func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) { plog("{\"type\":\"nav_fail_prov\",\"e\":\"\(error)\"}") }
}
