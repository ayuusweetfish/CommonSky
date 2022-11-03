// For flic.kr short links
const base58 = '123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ'
let n = +Deno.args[0]
let s = ''
do {
  s = base58[n % 58] + s
  n = Math.floor(n / 58)
} while (n > 0)
console.log(s)
