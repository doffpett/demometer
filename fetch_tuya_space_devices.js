const crypto = require('crypto');
const https = require('https');
const fs = require('fs');
const path = require('path');

// Load environment variables from .env if present
try {
  const envPath = path.join(__dirname, '.env');
  if (fs.existsSync(envPath)) {
    const envContent = fs.readFileSync(envPath, 'utf8');
    envContent.split(/\r?\n/).forEach(line => {
      const match = line.match(/^\s*([A-Za-z0-9_]+)\s*=\s*(.*)\s*$/);
      if (match) {
        const key = match[1];
        const value = match[2];
        if (!process.env[key]) {
          process.env[key] = value;
        }
      }
    });
  }
} catch (err) {
  console.warn('Could not load .env file:', err.message);
}

const region = process.env.TUYA_REGION || 'eu';
const clientId = process.env.TUYA_CLIENT_ID;
const secret = process.env.TUYA_CLIENT_SECRET;
const accessToken = process.env.TUYA_ACCESS_TOKEN;
const refreshToken = process.env.TUYA_REFRESH_TOKEN;

if (!clientId || !secret || !accessToken || !refreshToken) {
  console.error('Set TUYA_CLIENT_ID, TUYA_CLIENT_SECRET, TUYA_ACCESS_TOKEN and TUYA_REFRESH_TOKEN env vars');
  process.exit(1);
}

const method = 'GET';
const pathWithQuery = '/v2.0/cloud/thing/space/device?is_recursion=false&page_size=20&space_ids=235737481';

const bodyHash = crypto.createHash('sha256').update('').digest('hex');
const stringToSign = [method, bodyHash, '', pathWithQuery].join('\n');
const t = Date.now().toString();
const str = clientId + accessToken + t + stringToSign;
const signature = crypto.createHmac('sha256', secret).update(str).digest('hex').toUpperCase();

const options = {
  hostname: `openapi.tuya${region}.com`,
  path: pathWithQuery,
  method,
  headers: {
    'client_id': clientId,
    'access_token': accessToken,
    't': t,
    'sign_method': 'HMAC-SHA256',
    'sign': signature
  }
};

console.log('Making request to', `https://${options.hostname}${options.path}`);

const req = https.request(options, res => {
  let data = '';
  res.on('data', chunk => { data += chunk; });
  res.on('end', () => {
    console.log('Status:', res.statusCode);
    console.log('Body:', data);
  });
});

req.on('error', err => { console.error('Request error:', err.message); });
req.end();
