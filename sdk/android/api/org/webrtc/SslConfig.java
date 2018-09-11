/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.Collections;
import java.util.List;
import javax.annotation.Nullable;

/**
 * Java version of rtc::SSLConfig.
 *
 * Contains the configuration of any SSL/TLS connections that are initiated by
 * our client.
 */
public class SslConfig {
  /** Tracks rtc::TlsCertPolicy */
  public enum TlsCertPolicy {
    TLS_CERT_POLICY_SECURE,
    TLS_CERT_POLICY_INSECURE_NO_CHECK,
  }

  /** Indicates whether to enable OCSP stapling in TLS. */
  public final boolean enableOcspStapling;
  /** Indicates whether to enable the signed certificate timestamp extension in TLS. */
  public final boolean enableSignedCertTimestamp;
  /** Indicates whether to enable the TLS Channel ID extension. */
  public final boolean enableTlsChannelId;
  /** Indicates whether to enable the TLS GREASE extension. */
  public final boolean enableGrease;

  /** Indicates how to process TURN server certificates */
  public final TlsCertPolicy tlsCertPolicy;

  /**
   * Highest supported SSL version, as defined in the supported_versions TLS extension.
   * If null, the default OpenSSL/BoringSSL max version will be used.
   */
  @Nullable public final Integer maxSslVersion;

  /**
   * List of protocols to be used in the TLS ALPN extension.
   * If null, the default list of OpenSSL/BoringSSL ALPN protocols will be used.
   */
  @Nullable public final List<String> tlsAlpnProtocols;

  /**
   * List of elliptic curves to be used in the TLS elliptic curves extension.
   * Only curve names supported by OpenSSL should be used (eg. "P-256","X25519").
   * If null, the default list of OpenSSL/BoringSSL curves will be used.
   */
  @Nullable public final List<String> tlsEllipticCurves;

  private SslConfig(boolean enableOcspStapling, boolean enableSignedCertTimestamp,
      boolean enableTlsChannelId, boolean enableGrease, TlsCertPolicy tlsCertPolicy,
      Integer maxSslVersion, List<String> tlsAlpnProtocols, List<String> tlsEllipticCurves) {
    this.enableOcspStapling = enableOcspStapling;
    this.enableSignedCertTimestamp = enableSignedCertTimestamp;
    this.enableTlsChannelId = enableTlsChannelId;
    this.enableGrease = enableGrease;
    this.tlsCertPolicy = tlsCertPolicy;
    this.maxSslVersion = maxSslVersion;
    if (tlsAlpnProtocols != null) {
      this.tlsAlpnProtocols = Collections.unmodifiableList(tlsAlpnProtocols);
    } else {
      this.tlsAlpnProtocols = null;
    }
    if (tlsEllipticCurves != null) {
      this.tlsEllipticCurves = Collections.unmodifiableList(tlsEllipticCurves);
    } else {
      this.tlsEllipticCurves = null;
    }
  }

  @Override
  public String toString() {
    return "[enableOcspStapling=" + enableOcspStapling + "] [enableSignedCertTimestamp="
        + enableSignedCertTimestamp + "] [enableTlsChannelId=" + enableTlsChannelId
        + "] [enableGrease=" + enableGrease + "] [tlsCertPolicy=" + tlsCertPolicy
        + "] [maxSslVersion=" + maxSslVersion + "] [tlsAlpnProtocols=" + tlsAlpnProtocols
        + "] [tlsEllipticCurves=" + tlsEllipticCurves + "]";
  }

  public static Builder builder() {
    return new Builder();
  }

  public static class Builder {
    private boolean enableOcspStapling;
    private boolean enableSignedCertTimestamp;
    private boolean enableTlsChannelId;
    private boolean enableGrease;
    private TlsCertPolicy tlsCertPolicy;
    @Nullable private Integer maxSslVersion;
    @Nullable private List<String> tlsAlpnProtocols;
    @Nullable private List<String> tlsEllipticCurves;

    private Builder() {
      this.enableOcspStapling = true;
      this.enableSignedCertTimestamp = true;
      this.enableTlsChannelId = false;
      this.enableGrease = false;
      this.tlsCertPolicy = TlsCertPolicy.TLS_CERT_POLICY_SECURE;
      this.maxSslVersion = null;
      this.tlsAlpnProtocols = null;
      this.tlsEllipticCurves = null;
    }

    public Builder setEnableOcspStapling(boolean enableOcspStapling) {
      this.enableOcspStapling = enableOcspStapling;
      return this;
    }

    public Builder setEnableSignedCertTimestamp(boolean enableSignedCertTimestamp) {
      this.enableSignedCertTimestamp = enableSignedCertTimestamp;
      return this;
    }

    public Builder setEnableTlsChannelId(boolean enableTlsChannelId) {
      this.enableTlsChannelId = enableTlsChannelId;
      return this;
    }

    public Builder setEnableGrease(boolean enableGrease) {
      this.enableGrease = enableGrease;
      return this;
    }

    public Builder setTlsCertPolicy(TlsCertPolicy tlsCertPolicy) {
      this.tlsCertPolicy = tlsCertPolicy;
      return this;
    }

    public Builder setMaxSslVersion(int maxSslVersion) {
      this.maxSslVersion = maxSslVersion;
      return this;
    }

    public Builder setTlsAlpnProtocols(List<String> tlsAlpnProtocols) {
      this.tlsAlpnProtocols = tlsAlpnProtocols;
      return this;
    }

    public Builder setTlsEllipticCurves(List<String> tlsEllipticCurves) {
      this.tlsEllipticCurves = tlsEllipticCurves;
      return this;
    }

    public SslConfig createSslConfig() {
      return new SslConfig(enableOcspStapling, enableSignedCertTimestamp, enableTlsChannelId,
          enableGrease, tlsCertPolicy, maxSslVersion, tlsAlpnProtocols, tlsEllipticCurves);
    }
  }

  @CalledByNative
  boolean getEnableOcspStapling() {
    return enableOcspStapling;
  }

  @CalledByNative
  boolean getEnableSignedCertTimestamp() {
    return enableSignedCertTimestamp;
  }

  @CalledByNative
  boolean getEnableTlsChannelId() {
    return enableTlsChannelId;
  }

  @CalledByNative
  boolean getEnableGrease() {
    return enableGrease;
  }

  @CalledByNative
  TlsCertPolicy getTlsCertPolicy() {
    return tlsCertPolicy;
  }

  @Nullable
  @CalledByNative
  Integer getMaxSslVersion() {
    return maxSslVersion;
  }

  @Nullable
  @CalledByNative
  List<String> getTlsAlpnProtocols() {
    return tlsAlpnProtocols;
  }

  @Nullable
  @CalledByNative
  List<String> getTlsEllipticCurves() {
    return tlsEllipticCurves;
  }
}
