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
    func makeUIView(context: Context) -> UIView { host.container }
    func updateUIView(_ uiView: UIView, context: Context) {}
}

func plog(_ s: String) {
    print("PANDA|" + s)
    fflush(stdout)
}

// Serves the bundled Site/ folder under panda://app/ (any path maps to Site/<path>)
final class SiteHandler: NSObject, WKURLSchemeHandler {
    let root = Bundle.main.url(forResource: "Site", withExtension: nil)!
    // Files in the app's Documents folder (copied there with `devicectl device copy to`), served under panda://app/docs/<name> in 4 MB pieces
    // with Range support, so a 512 MB game never has to sit in memory as one Data object.
    func serveDoc(_ name: String, _ task: WKURLSchemeTask) {
        let url = task.request.url!
        let docs = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        let path = docs.appendingPathComponent(name)
        guard let fh = try? FileHandle(forReadingFrom: path),
              let attrs = try? FileManager.default.attributesOfItem(atPath: path.path),
              let size = (attrs[.size] as? NSNumber)?.uint64Value, size > 0 else {
            task.didReceive(HTTPURLResponse(url: url, statusCode: 404, httpVersion: "HTTP/1.1", headerFields: nil)!)
            task.didFinish(); return
        }
        var start: UInt64 = 0, end = size - 1, status = 200
        if let r = task.request.value(forHTTPHeaderField: "Range"), r.hasPrefix("bytes=") {
            let parts = r.dropFirst(6).split(separator: "-", omittingEmptySubsequences: false)
            if let a = parts.first.flatMap({ UInt64($0) }) { start = a; status = 206 }
            if parts.count > 1, let b = UInt64(parts[1]) { end = min(b, size - 1) }
        }
        let length = end >= start ? end - start + 1 : 0
        var headers = ["Content-Type": "application/octet-stream", "Content-Length": "\(length)", "Accept-Ranges": "bytes", "Cache-Control": "no-store"]
        if status == 206 { headers["Content-Range"] = "bytes \(start)-\(end)/\(size)" }
        task.didReceive(HTTPURLResponse(url: url, statusCode: status, httpVersion: "HTTP/1.1", headerFields: headers)!)
        try? fh.seek(toOffset: start)
        var remaining = length
        while remaining > 0 {
            let ok: Bool = autoreleasepool {
                guard let d = try? fh.read(upToCount: Int(min(remaining, 4 * 1024 * 1024))), !d.isEmpty else { return false }
                task.didReceive(d); remaining -= UInt64(d.count); return true
            }
            if !ok { break }
        }
        try? fh.close()
        task.didFinish()
    }

    func webView(_ webView: WKWebView, start task: WKURLSchemeTask) {
        let url = task.request.url!
        let rel = String(url.path.dropFirst())
        if rel.hasPrefix("docs/") { serveDoc(String(rel.dropFirst(5)), task); return }
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
    let container = UIView()
    var webView: WKWebView!
    var timer: Timer?
    // playlist mode (-playlist a.3dsx,b.3dsx,...): every ROM runs in a fresh WKWebView (fresh WebContent process); the native side judges the outcome
    var playlist: [String] = []
    var idx = 0, okCount = 0, failCount = 0
    var itemName = "", itemROM = ""
    var itemStart = Date(), lastMsg = Date()
    var lastFrame = -1, itemWarnings = 0
    var itemDone = true
    var stallSec = 6.0, maxSec = 120.0
    var watchdog: Timer?

    override init() {
        super.init()
        container.backgroundColor = .black
        makeWebView()
    }

    func makeWebView() {
        if let old = webView {
            old.stopLoading()
            old.configuration.userContentController.removeScriptMessageHandler(forName: "panda")
            old.navigationDelegate = nil
            old.removeFromSuperview()
        }
        let cfg = WKWebViewConfiguration()
        cfg.setURLSchemeHandler(SiteHandler(), forURLScheme: "panda")
        let uc = WKUserContentController()
        cfg.userContentController = uc
        uc.add(self, name: "panda")
        let w = WKWebView(frame: container.bounds, configuration: cfg)
        w.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        w.navigationDelegate = self
        w.isInspectable = true
        w.scrollView.isScrollEnabled = false
        w.scrollView.bounces = false
        w.backgroundColor = .black
        w.isOpaque = true
        container.addSubview(w)
        webView = w
    }

    func query(rom: String, name: String) -> String {
        let d = UserDefaults.standard
        let frames = d.string(forKey: "frames") ?? "0"
        let turbo = d.string(forKey: "turbo") ?? "0"
        let report = d.string(forKey: "report") ?? "1"
        let dbg = d.string(forKey: "dbg") ?? "0"
        let env = d.string(forKey: "env") ?? ""
        let gl = d.string(forKey: "gl") ?? "0"
        let sd = d.string(forKey: "sd") ?? ""
        let script = d.string(forKey: "script") ?? ""
        let shots = d.string(forKey: "shots") ?? "0"
        let pace = d.string(forKey: "pace") ?? ""
        return "rom=\(rom)&frames=\(frames)&turbo=\(turbo)&name=\(name)&report=\(report)&dbg=\(dbg)&env=\(env)&gl=\(gl)&sd=\(sd)&script=\(script)&shots=\(shots)&pace=\(pace)"
    }

    func start() {
        UIApplication.shared.isIdleTimerDisabled = true
        let d = UserDefaults.standard
        var info = utsname(); uname(&info)
        let model = withUnsafePointer(to: &info.machine) { $0.withMemoryRebound(to: CChar.self, capacity: 64) { String(cString: $0) } }
        timer = Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { _ in
            plog("{\"type\":\"native\",\"thermal\":\(ProcessInfo.processInfo.thermalState.rawValue),\"appAvailMB\":\(os_proc_available_memory() / 1_048_576),\"lowPower\":\(ProcessInfo.processInfo.isLowPowerModeEnabled)}")
        }
        if let pl = d.string(forKey: "playlist"), !pl.isEmpty {
            playlist = pl.split(separator: ",").map(String.init)
            stallSec = Double(d.string(forKey: "stall") ?? "") ?? 6.0
            maxSec = Double(d.string(forKey: "max") ?? "") ?? 120.0
            plog("{\"type\":\"playlist_start\",\"count\":\(playlist.count),\"ios\":\"\(UIDevice.current.systemVersion)\",\"model\":\"\(model)\",\"stallSec\":\(stallSec)}")
            watchdog = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in self?.checkWatchdog() }
            runItem()
            return
        }
        let rom = d.string(forKey: "rom") ?? "/run/ctr-hello.3dsx"
        let frames = d.string(forKey: "frames") ?? "0"
        let turbo = d.string(forKey: "turbo") ?? "0"
        let name = d.string(forKey: "name") ?? "run"
        plog("{\"type\":\"launch\",\"name\":\"\(name)\",\"ios\":\"\(UIDevice.current.systemVersion)\",\"model\":\"\(model)\",\"rom\":\"\(rom)\",\"frames\":\(frames),\"turbo\":\(turbo)}")
        let page = d.string(forKey: "page") ?? "index.html"
        let raw = d.string(forKey: "q") ?? query(rom: rom, name: name)   // `-q "n=8192&mode=inst"` replaces the generated query (for stress pages)
        webView.load(URLRequest(url: URL(string: "panda://app/web/\(page)?\(raw)")!))
    }

    // ---- playlist ----
    func runItem() {
        itemROM = playlist[idx]
        itemName = ((itemROM as NSString).lastPathComponent as NSString).deletingPathExtension
        itemDone = false; lastFrame = -1; itemWarnings = 0
        itemStart = Date(); lastMsg = Date()
        makeWebView()
        webView.load(URLRequest(url: URL(string: "panda://app/web/index.html?\(query(rom: itemROM, name: itemName))")!))
    }

    func finishItem(_ result: String, _ detail: String) {
        if itemDone { return }
        itemDone = true
        let ok = result == "DONE"
        if ok { okCount += 1 } else { failCount += 1 }
        let esc = String(detail.unicodeScalars.filter { $0.value >= 32 }.map(Character.init)).replacingOccurrences(of: "\\", with: "/").replacingOccurrences(of: "\"", with: "'")
        plog("{\"type\":\"playlist_item\",\"i\":\(idx + 1),\"of\":\(playlist.count),\"name\":\"\(itemName)\",\"result\":\"\(result)\",\"detail\":\"\(esc)\",\"lastFrame\":\(lastFrame),\"sec\":\(String(format: "%.1f", Date().timeIntervalSince(itemStart)))}")
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { [self] in
            idx += 1
            if idx < playlist.count { runItem() }
            else {
                plog("{\"type\":\"playlist_done\",\"ok\":\(okCount),\"failed\":\(failCount),\"count\":\(playlist.count)}")
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { exit(0) }
            }
        }
    }

    func checkWatchdog() {
        if itemDone || playlist.isEmpty { return }
        let now = Date()
        if now.timeIntervalSince(lastMsg) > stallSec { finishItem("HANG", "no message from the page for \(Int(stallSec)) s") }
        else if now.timeIntervalSince(itemStart) > maxSec { finishItem("HANG", "item exceeded \(Int(maxSec)) s") }
    }

    func userContentController(_ c: WKUserContentController, didReceive m: WKScriptMessage) {
        guard m.webView === webView, let s = m.body as? String else { return }
        let obj = (try? JSONSerialization.jsonObject(with: Data(s.utf8))) as? [String: Any]
        let type = obj?["type"] as? String ?? ""
        if playlist.isEmpty {
            plog(s)
            if type == "done" { DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { exit(0) } }
            return
        }
        // playlist mode: keep the page's own lines out of the log except the interesting ones; judge the outcome here
        lastMsg = Date()
        if type == "frame" { lastFrame = obj?["n"] as? Int ?? lastFrame }
        if type == "crumb" || type == "frame" { return }
        if type == "shot" { plog(s); return }   // canvas screenshot (gl=1 + report=1): the host script saves it
        if type == "done" {
            let g = { (k: String) -> String in
                if let v = obj?[k] as? Double { return String(format: "%.1f", v) }
                if let v = obj?[k] as? Int { return "\(v)" }
                return "?"
            }
            finishItem("DONE", "fps=\(g("fps")) avg=\(g("avgMs"))ms worst=\(g("worstMs"))ms modules=\(g("blocks")) jitKB=\(g("jitKB")) warnings=\(itemWarnings)")
        } else if type == "error" {
            let msg = obj?["msg"] as? String ?? s
            if msg.contains("[FATAL]") { finishItem("ERROR", msg) } else { itemWarnings += 1 }
        } else if type == "log" {
            let msg = obj?["msg"] as? String ?? ""
            if msg.contains("[FATAL]") { plog(s) }
        }
    }
    func webViewWebContentProcessDidTerminate(_ webView: WKWebView) {
        plog("{\"type\":\"webcontent_terminated\"}")
        if playlist.isEmpty { DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) { exit(2) } }
        else { finishItem("KILLED", "WebContent process terminated") }
    }
    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) { plog("{\"type\":\"nav_fail\",\"e\":\"\(error)\"}") }
    func webView(_ webView: WKWebView, didFailProvisionalNavigation navigation: WKNavigation!, withError error: Error) { plog("{\"type\":\"nav_fail_prov\",\"e\":\"\(error)\"}") }
}
