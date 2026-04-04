#pragma once

namespace p2p::utils {

/**
 * Add standard security headers to HTTP response
 *
 * Headers added:
 * - X-Frame-Options: DENY (prevent clickjacking)
 * - X-Content-Type-Options: nosniff (prevent MIME sniffing)
 * - X-XSS-Protection: 1; mode=block (enable XSS filter)
 * - Content-Security-Policy: default-src 'self' (restrict resource loading)
 * - Strict-Transport-Security: max-age=31536000 (enforce HTTPS)
 * - Referrer-Policy: strict-origin-when-cross-origin (control referrer info)
 */
template<typename Response>
void AddSecurityHeaders(Response& res) {
    res.set("X-Frame-Options", "DENY");
    res.set("X-Content-Type-Options", "nosniff");
    res.set("X-XSS-Protection", "1; mode=block");
    res.set("Content-Security-Policy", "default-src 'self'");
    res.set("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    res.set("Referrer-Policy", "strict-origin-when-cross-origin");
}

} // namespace p2p::utils
