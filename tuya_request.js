const crypto = require('crypto');
const https = require('https');

const method = (process.argv[2] || '').toUpperCase();
const pathWithQuery = process.argv[3];
const bodyArg = process.argv[4] || '';

if (!method || !pathWithQuery) {
  console.error('Usage: node tuya_request.js <METHOD> <PATH_WITH_QUERY> [BODY]');
  process.exit(1);
}

const region = process.env.TUYA_REGION || 'eu';
const clientId = process.env.TUYA_CLIENT_ID;
const secret = process.env.TUYA_CLIENT_SECRET;
const accessToken = process.env.TUYA_ACCESS_TOKEN;

if (!clientId || !secret || !accessToken) {
  console.error('Set TUYA_CLIENT_ID, TUYA_CLIENT_SECRET and TUYA_ACCESS_TOKEN env vars');
  process.exit(1);
}

const body = bodyArg;
const bodyHash = crypto.createHash('sha256').update(body).digest('hex');

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

if (body) {
  options.headers['Content-Type'] = 'application/json';
  options.headers['Content-Length'] = Buffer.byteLength(body);
}

console.log('Request options:', options);
if (body) console.log('Request body:', body);

const req = https.request(options, res => {
  let data = '';
  res.on('data', chunk => {
    data += chunk;
  });
  res.on('end', () => {
    console.log('Response:', res.statusCode, data);
  });
});

req.on('error', err => {
  console.error('Request error:', err.message);
});

if (body) {
  req.write(body);
}
req.end();
