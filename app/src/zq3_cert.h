/*
 * SPDX-FileCopyrightText: Copyright 2025 Sam Blenny
 * SPDX-License-Identifier: Apache-2.0
 *
 * Docs & Refs:
 * zephyr/samples/net/secure_mqtt_sensor_actuator/src/mqtt_client.c
 * zephyr/samples/net/secure_mqtt_sensor_actuator/src/tls_config/cert.h
 * zephyr/include/zephyr/net/tls_credentials.h
 *
 * DigiCert Root Authority Certificates download page
 * https://www.digicert.com/kb/digicert-root-certificates.htm
 */

#ifndef ZQ3_CERT_H
#define ZQ3_CERT_H

#include <zephyr/net/tls_credentials.h>


/*
 * This array is used in initializing the mqtt_sec_config struct as part of
 * setting up TLS for the MQTT connection. The tag numbers need to be unique,
 * and there should be one tag for each CA cert.
 */
static const sec_tag_t zq3_cert_tags[] = {1, 2, 3};

/*
 * This is the certificate for my self-signed CA, so it's useless to anybody
 * who isn't talking to the mosquitto broker on my private test network.
 *
 * But, if you want to make your own self-signed certificate, you could paste
 * that certificate here in place of mine. If you use 2048-bit RSA, it will
 * probably just work. For other key sizes or algorithms, you might need to
 * make adjustments to the code or Kconfig settings.
 */
static const char zq3_cert_self_signed[] =
	"-----BEGIN CERTIFICATE-----\r\n"
	"MIIDUTCCAjmgAwIBAgIUVUPZ9jLPLAFUurAcLfZNyliYmLMwDQYJKoZIhvcNAQEL\r\n"
	"BQAwODELMAkGA1UEBhMCVVMxDTALBgNVBAoMBE15Q0ExGjAYBgNVBAMMEU15IFNl\r\n"
	"bGYtU2lnbmVkIENBMB4XDTI1MDMyNDA3MTYxMloXDTI3MDMyNDA3MTYxMlowODEL\r\n"
	"MAkGA1UEBhMCVVMxDTALBgNVBAoMBE15Q0ExGjAYBgNVBAMMEU15IFNlbGYtU2ln\r\n"
	"bmVkIENBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA2rBJgpv+OyBr\r\n"
	"iO8HjZatn0DSWNFGsn5tl1hNl9chLbQCdlTO1rWKhUf/x9YYelO7C7CRzIZT7HYA\r\n"
	"eXxCQg+e0uf8Ufq9ZedGBhXJymBIIOoJpX9K4jYAMf6YVYsjjGgFmoiqLAIIJmuS\r\n"
	"+e2RbXc9kqCN/phe8XagqKKMfBBMQID+9vn6I1Was4uLDuulfFAq0+tJY+987pYw\r\n"
	"zTL9van5Q+/BlhKnScv75c9blnPfYfE0nrh0zXnEziGFgfPYD2lMYnMM4rMw3+AQ\r\n"
	"BREW3cevQs6AHvL/X+NuLwudIEShX/6+k8RI4I7GaC4MiUopYguFbhMgqh/kf7Rx\r\n"
	"dJygbrCLOQIDAQABo1MwUTAdBgNVHQ4EFgQU1Uq+LvLFqFu3/DSPWBe8PGnKG+Uw\r\n"
	"HwYDVR0jBBgwFoAU1Uq+LvLFqFu3/DSPWBe8PGnKG+UwDwYDVR0TAQH/BAUwAwEB\r\n"
	"/zANBgkqhkiG9w0BAQsFAAOCAQEAabXpeZJC8ZJBnqZNozFdOgfRTAczoW2buZ8P\r\n"
	"RmchlSSPhxKsh6gXHK0rr3qF2YKvKiTpp4d2g5DAse5ofhKk8kUB7zdaYfzED5wq\r\n"
	"QCEvvyPF56WJYg3VCkP4Gn2fwCPBek3++gDiqKBNiZsV0teOZ/NoI5q5UIT28E/e\r\n"
	"9qKd5xliqLbv6XJXI1SLc5Ftp9flmKjIwUL7A5sG6Q2lHAoS+t9ha99yEb87s/qr\r\n"
	"Xmu9aU6/B6PFf3M5XAbV5AlGsjD3Y6grn3DKJwKoMDvzCBE7XCYO2g/nHGLqYnwX\r\n"
	"sSnqXwvCR2cWE4z+dTSa9KeAhgt3qws9VnrmnK2c13qNKl9eQQ==\r\n"
	"-----END CERTIFICATE-----\r\n";

/*
 * This is the "DigiCert Global Root G2" certificate, which signs the
 * intermediate certificate, which signs the Adafruit IO server certificate
 * (as of March 2025). This is good until Jan 15, 2038. The public key is
 * 2048-bit RSA. I downloaded this from:
 *    https://www.digicert.com/kb/digicert-root-certificates.htm
 *
 * CAUTION: For mbed TLS to verify the server's certificate, it doesn't work
 * to just give an intermediate certificate here. You also need to give the
 * first certificate in the chain (root cert).
 */
static const char zq3_cert_digicert_global_root_g2[] =
	"-----BEGIN CERTIFICATE-----\r\n"
	"MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\r\n"
	"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\r\n"
	"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\r\n"
	"MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\r\n"
	"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\r\n"
	"b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\r\n"
	"9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\r\n"
	"2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\r\n"
	"1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\r\n"
	"q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\r\n"
	"tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\r\n"
	"vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\r\n"
	"BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\r\n"
	"5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\r\n"
	"1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\r\n"
	"NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\r\n"
	"Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\r\n"
	"8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\r\n"
	"pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\r\n"
	"MrY=\r\n"
	"-----END CERTIFICATE-----\r\n";

/*
 * This is the "GeoTrust TLS RSA CA G1" intermediate certificate which signs
 * the current Adafruit IO server certificate (as of March 2025). The end date
 * for this one is Nov 2, 2027. The public key is 2048-bit RSA.
 *
 * You can check the current Adafruit IO certificate from a terminal:
 *    openssl s_client -showcerts -connect io.adafruit.com:8883
 */
static const char zq3_cert_geotrust_g1[] =
	"-----BEGIN CERTIFICATE-----\r\n"
	"MIIEjTCCA3WgAwIBAgIQDQd4KhM/xvmlcpbhMf/ReTANBgkqhkiG9w0BAQsFADBh\r\n"
	"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\r\n"
	"d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\r\n"
	"MjAeFw0xNzExMDIxMjIzMzdaFw0yNzExMDIxMjIzMzdaMGAxCzAJBgNVBAYTAlVT\r\n"
	"MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\r\n"
	"b20xHzAdBgNVBAMTFkdlb1RydXN0IFRMUyBSU0EgQ0EgRzEwggEiMA0GCSqGSIb3\r\n"
	"DQEBAQUAA4IBDwAwggEKAoIBAQC+F+jsvikKy/65LWEx/TMkCDIuWegh1Ngwvm4Q\r\n"
	"yISgP7oU5d79eoySG3vOhC3w/3jEMuipoH1fBtp7m0tTpsYbAhch4XA7rfuD6whU\r\n"
	"gajeErLVxoiWMPkC/DnUvbgi74BJmdBiuGHQSd7LwsuXpTEGG9fYXcbTVN5SATYq\r\n"
	"DfbexbYxTMwVJWoVb6lrBEgM3gBBqiiAiy800xu1Nq07JdCIQkBsNpFtZbIZhsDS\r\n"
	"fzlGWP4wEmBQ3O67c+ZXkFr2DcrXBEtHam80Gp2SNhou2U5U7UesDL/xgLK6/0d7\r\n"
	"6TnEVMSUVJkZ8VeZr+IUIlvoLrtjLbqugb0T3OYXW+CQU0kBAgMBAAGjggFAMIIB\r\n"
	"PDAdBgNVHQ4EFgQUlE/UXYvkpOKmgP792PkA76O+AlcwHwYDVR0jBBgwFoAUTiJU\r\n"
	"IBiV5uNu5g/6+rkS7QYXjzkwDgYDVR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQGCCsG\r\n"
	"AQUFBwMBBggrBgEFBQcDAjASBgNVHRMBAf8ECDAGAQH/AgEAMDQGCCsGAQUFBwEB\r\n"
	"BCgwJjAkBggrBgEFBQcwAYYYaHR0cDovL29jc3AuZGlnaWNlcnQuY29tMEIGA1Ud\r\n"
	"HwQ7MDkwN6A1oDOGMWh0dHA6Ly9jcmwzLmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEds\r\n"
	"b2JhbFJvb3RHMi5jcmwwPQYDVR0gBDYwNDAyBgRVHSAAMCowKAYIKwYBBQUHAgEW\r\n"
	"HGh0dHBzOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwDQYJKoZIhvcNAQELBQADggEB\r\n"
	"AIIcBDqC6cWpyGUSXAjjAcYwsK4iiGF7KweG97i1RJz1kwZhRoo6orU1JtBYnjzB\r\n"
	"c4+/sXmnHJk3mlPyL1xuIAt9sMeC7+vreRIF5wFBC0MCN5sbHwhNN1JzKbifNeP5\r\n"
	"ozpZdQFmkCo+neBiKR6HqIA+LMTMCMMuv2khGGuPHmtDze4GmEGZtYLyF8EQpa5Y\r\n"
	"jPuV6k2Cr/N3XxFpT3hRpt/3usU/Zb9wfKPtWpoznZ4/44c1p9rzFcZYrWkj3A+7\r\n"
	"TNBJE0GmP2fhXhP1D/XVfIW/h0yCJGEiV9Glm/uGOa3DXHlmbAcxSyCRraG+ZBkA\r\n"
	"7h4SeM6Y8l/7MBRpPCz6l8Y=\r\n"
	"-----END CERTIFICATE-----\r\n";


#endif /* ZQ3_CERT_H */
