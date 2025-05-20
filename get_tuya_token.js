const crypto = require('crypto');
const https = require('https');
const fs = require('fs');
const path = require('path');

// Load environment variables from .env if the file exists
const envPath = path.join(__dirname, '.env');
if (fs.existsSync(envPath)) {
  const envData = fs.readFileSync(envPath, 'utf8');
  for (const line of envData.split(/\r?\n/)) {
    const match = line.match(/^([^#=]+)=([\s\S]*)$/);
    if (match) {
      const key = match[1].trim();
      const value = match[2].trim();
      if (!process.env[key]) {
        process.env[key] = value;
      }
    }
  }
}

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
