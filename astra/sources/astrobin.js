for (const path of Deno.args) {
  const [id, rev] = path.split('/')
/*
  const resp = await fetch(
    `https://www.astrobin.com/full/${id}/${rev || 0}/`
  )
  const data = await resp.text()
  for (const line of data.split('\n'))
    if (line.indexOf('data-alias') !== -1)
      console.log(line)
*/
  for (const alias of ['real', 'qhd', 'qhd_sharpened']) {
    const resp = await fetch(
      `https://www.astrobin.com/${id}/${rev || 0}/thumb/${alias}/`
    )
    try {
      const data = await resp.json()
      const filename = data.url.match(/[^/]+\.[a-z]+$/)[0]
      const fileid = filename.match(/^.+(?=_[0-9]+x[0-9]+_)/g)[0]
      console.log(path, alias, fileid)
    } catch (e) {
      // Empty
    }
  }
}
