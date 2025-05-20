const crypto = require('crypto');
const https = require('https');
const fs = require('fs');
const path = require('path');

function loadEnv() {
  const envPath = path.join(__dirname, '.env');
  if (!fs.existsSync(envPath)) return;
  const lines = fs.readFileSync(envPath, 'utf8').split('\n');
  for (const line of lines) {
    const m = line.match(/^\s*([A-Za-z0-9_]+)\s*=\s*(.*)\s*$/);
    if (m) process.env[m[1]] = m[2];
  }
}

loadEnv();

const region = process.env.TUYA_REGION || 'eu';
const clientId = process.env.TUYA_CLIENT_ID;
const secret = process.env.TUYA_CLIENT_SECRET;

if (!clientId || !secret) {
  console.error('Set TUYA_CLIENT_ID and TUYA_CLIENT_SECRET environment variables');
  process.exit(1);
}

const HTTP_METHOD = 'GET';
const PATH_WITH_QUERY = '/v1.0/token?grant_type=1';
const NULL_HASH = 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855';

const t = Date.now().toString();
const stringToSign = [HTTP_METHOD, NULL_HASH, '', PATH_WITH_QUERY].join('\n');
const str = clientId + t + '' + stringToSign;

const hmac = crypto.createHmac('sha256', secret);
hmac.update(str);
const signature = hmac.digest('hex').toUpperCase();

const options = {
  hostname: `openapi.tuya${region}.com`,
  path: PATH_WITH_QUERY,
  method: HTTP_METHOD,
  headers: {
    'client_id': clientId,
    't': t,
    'sign_method': 'HMAC-SHA256',
    'sign': signature
  }
};

console.log('Request options:', options);

const req = https.request(options, res => {
  let data = '';
  res.on('data', chunk => { data += chunk; });
  res.on('end', () => { console.log('Response:', res.statusCode, data); });
});

req.on('error', err => { console.error('Request error:', err.message); });
req.end();
