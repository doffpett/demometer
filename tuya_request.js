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
const accessToken = process.env.TUYA_ACCESS_TOKEN;

if (!clientId || !secret || !accessToken) {
  console.error('Set TUYA_CLIENT_ID, TUYA_CLIENT_SECRET and TUYA_ACCESS_TOKEN');
  process.exit(1);
}

const method = (process.argv[2] || 'GET').toUpperCase();
const urlPathAndQuery = process.argv[3];
if (!urlPathAndQuery) {
  console.error('Usage: node tuya_request.js <METHOD> <PATH_WITH_QUERY>');
  process.exit(1);
}

const nonce = crypto.randomBytes(4).toString('hex');
const NULL_HASH = 'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855';
const t = Date.now().toString();
const stringToSign = [method, NULL_HASH, '', urlPathAndQuery].join('\n');
const str = clientId + accessToken + t + nonce + stringToSign;

const hmac = crypto.createHmac('sha256', secret);
hmac.update(str);
const signature = hmac.digest('hex').toUpperCase();

const options = {
  hostname: `openapi.tuya${region}.com`,
  path: urlPathAndQuery,
  method,
  headers: {
    'client_id': clientId,
    'access_token': accessToken,
    't': t,
    'nonce': nonce,
    'sign_method': 'HMAC-SHA256',
    'sign': signature
  }
};

console.log('Request options:', options);

const req = https.request(options, res => {
  let data = '';
  res.on('data', chunk => { data += chunk; });
  res.on('end', () => {
    console.log('Response:', res.statusCode, data);
  });
});

req.on('error', err => {
  console.error('Request error:', err.message);
});
req.end();
