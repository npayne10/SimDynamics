document.addEventListener('DOMContentLoaded', function(){

    // ====== CONFIG ======
    const CURRENCY = 'ZAR';
    const SANDBOX = true; // set to false for live
    // TODO: Replace with your credentials
    const MERCHANT_ID = '10000100'; // Sandbox example
    const MERCHANT_KEY = '46f0cd694581a'; // Sandbox example
    const RETURN_URL = 'https://example.com/return';
    const CANCEL_URL = 'https://example.com/cancel';
    const NOTIFY_URL = 'https://example.com/ipn';
    const PASSPHRASE = ''; // optional; if set, will be appended when creating signature

    // ====== SIMPLE PRODUCT CATALOG (edit freely) ======
    const PRODUCTS = [
      { id:'rig-basic',  name:'SymDynamics Racing Rig — Basic',  price: 19999.00, image:'RacingSim1.png', desc:'Entry‑level motion‑ready cockpit. Steel frame, seat, monitor mount.' },
      { id:'rig-pro',    name:'SymDynamics Racing Rig — Pro',    price: 42999.00, image:'', desc:'Pro‑grade motion platform prepared. Adjustable seat/controls.' },
      { id:'flight-kit', name:'Flight Controls Kit',             price: 9999.00,  image:'', desc:'Yoke/Throttle/Pedals bundle. Plug‑and‑play with top sims.' },
      { id:'dd-wheel',   name:'Direct Drive Wheel Base',         price: 13999.00, image:'', desc:'High‑torque DD base. Smooth, detailed force feedback.' },
      { id:'pedals-pro', name:'Load‑cell Pedals — 3 Piece',      price: 7499.00,  image:'', desc:'Progressive brake, fine throttle control. Tunable springs.' },
      { id:'vr-headset', name:'VR Headset + Controllers',        price: 12999.00, image:'', desc:'High‑res optics, wide FOV. Immersive racing/flight experience.' }
    ];

    // ====== AFFILIATE LINKS (edit freely) ======
    const AFFILIATES = [
      { id:'aff-pxn-vd6', name:'PXN V-D6 Racing Wheel + Pedals (PC)', desc:'Entry racing wheel with force feedback and dual-pedal set — great starter bundle.', url:'https://pxn-game.com/products/pxn-vd6-racing-wheel-and-pedals-for-pc?sca_ref=9793419.Ag4kWCw0ZVkt6MU', image:'PXN_VD6_W_DS_R2_Bundle_for_PC_with_Dual_Pedals_1500_1500.webp' },
      { id:'aff-fan-csldd', name:'Fanatec CSL DD Wheel Base', desc:'Direct-drive performance at a friendly price. Pair with your preferred rim.', url:'#YOUR_FANATEC_CSL_DD_LINK', image:'R2R_CSL_DD_QR2_PVGT_5NM-01.webp' },
      { id:'aff-fan-loadcell', name:'Fanatec CSL Pedals + Load Cell Kit', desc:'Affordable load‑cell braking for consistent lap times.', url:'#YOUR_FANATEC_LOADCELL_LINK', image:'' },
      { id:'aff-logi-g923', name:'Logitech G923 Wheel + Pedals', desc:'Reliable gear‑drive wheel with TrueForce effects — perfect for new racers.', url:'#YOUR_LOGITECH_G923_LINK', image:'' },
      { id:'aff-logi-pro', name:'Logitech PRO Racing Wheel (DD)', desc:'High‑torque direct‑drive base with console/PC variants.', url:'#YOUR_LOGITECH_PRO_WHEEL_LINK', image:'' },
      { id:'aff-tm-tlcm', name:'Thrustmaster T‑LCM Load‑Cell Pedals', desc:'Magnetic sensors + load‑cell brake. Strong value for mixed platforms.', url:'#YOUR_THRUSTMASTER_TLCM_LINK', image:'' },
      { id:'aff-meta-q3', name:'Meta Quest 3 (VR)', desc:'Great standalone VR for racing & flight; sharp display and rich ecosystem.', url:'#YOUR_META_QUEST3_LINK', image:'' },
      { id:'aff-hp-g2', name:'HP Reverb G2 (VR)', desc:'High clarity for flight sim cockpits; inside‑out tracking.', url:'#YOUR_HP_REVERB_G2_LINK', image:'' },
      { id:'aff-htc-pro2', name:'HTC Vive Pro 2 (VR)', desc:'Wide FOV and strong PC‑VR fidelity for sim enthusiasts.', url:'#YOUR_VIVE_PRO2_LINK', image:'' },
      { id:'aff-fan-shifter', name:'Fanatec ClubSport Shifter SQ V1.1', desc:'H‑pattern and sequential in one — metal construction.', url:'#YOUR_FANATEC_SHIFTER_LINK', image:'' },
      { id:'aff-cockpit', name:'Adjustable Sim Cockpit/Chassis', desc:'Sturdy, upgradeable base for wheel, pedals and seat — pick your vendor.', url:'#YOUR_COCKPIT_LINK', image:'' }
    ];

    // UTM config for affiliate links
    const UTM = { source: 'symdynamics', medium: 'affiliate', campaign: 'affiliates_page' };
    function addUtm(url, content){
      if(!url || url === '#' || url.startsWith('#YOUR_')) return url;
      const hasQuery = url.includes('?');
      const sep = hasQuery ? '&' : '?';
      return `${url}${sep}utm_source=${encodeURIComponent(UTM.source)}&utm_medium=${encodeURIComponent(UTM.medium)}&utm_campaign=${encodeURIComponent(UTM.campaign)}&utm_content=${encodeURIComponent(content)}`;
    }

    // ====== CART STATE ======
    const cart = JSON.parse(localStorage.getItem('sym_cart')||'{}');
    function saveCart(){ localStorage.setItem('sym_cart', JSON.stringify(cart)); updateCartBadge(); }

    // ====== USER (REGISTRATION) STATE ======
    function getUser(){ try { return JSON.parse(localStorage.getItem('sym_user')||'null'); } catch(e){ return null; } }
    function saveUser(u){ localStorage.setItem('sym_user', JSON.stringify(u)); updateNavUser(); updateShipTo(); }

    // ====== RENDER PRODUCTS ======
    const grid = document.getElementById('productGrid');
    if(grid){
      PRODUCTS.forEach(p=>{
        const el = document.createElement('div');
        el.className='product';
        el.innerHTML = `
          <div class=\"ph\">Image placeholder (${p.name})</div>
          <div class=\"body\">
            <strong>${p.name}</strong>
            <p style=\"color:#a7b0c2;margin:0\">${p.desc}</p>
            <div class=\"price\">R${p.price.toFixed(2)}</div>
            <div style=\"display:flex;gap:8px;align-items:center\">
              <label>Qty <input class=\"qty\" type=\"number\" min=\"1\" value=\"1\"></label>
              <button class=\"btn add\">Add to Cart</button>
            </div>
          </div>`;
        el.querySelector('.add').addEventListener('click',()=>{
          const qty = parseInt(el.querySelector('.qty').value||'1');
          cart[p.id] = (cart[p.id]||0) + Math.max(1, qty);
          saveCart();
          renderCart();
          location.hash = '#cart';
        });
        grid.appendChild(el);
      });
    }

    // ====== RENDER AFFILIATES ======
    const affGrid = document.getElementById('affGrid');
    // helper to pick an image: explicit > worker(OG) > domain favicon
    function affiliateImageUrl(a){
      if(a.image && a.image.trim() !== '') return a.image; // explicit
      try{
        const u = new URL(a.url);
        // favicon fallback (always works cross-origin)
        return `https://www.google.com/s2/favicons?domain=${u.hostname}&sz=128`;
      }catch(e){ return 'https://www.google.com/s2/favicons?sz=128&domain=example.com'; }
    }

    if(affGrid){
      AFFILIATES.forEach(a=>{
        const el = document.createElement('div');
        el.className='product';
        const imgSrc = affiliateImageUrl(a);
        el.innerHTML = `
          <img class="ph-img" src="${imgSrc}" alt="${a.name} logo/image" loading="lazy" referrerpolicy="no-referrer"/>
          <div class="body">
            <strong>${a.name}</strong>
            <p style="color:#a7b0c2;margin:0">${a.desc}</p>
            <div style="display:flex;gap:8px;align-items:center;justify-content:space-between">
              <a class="btn" target="_blank" rel="noopener" href="${addUtm(a.url,a.id)}">Visit</a>
              <span style="font-size:12px;color:#a7b0c2">Affiliate</span>
            </div>
          </div>`;
        affGrid.appendChild(el);
      });
      // show disclosure unless dismissed
      const dismissed = localStorage.getItem('aff_disclosure_dismissed') === '1';
      const banner = document.getElementById('affDisclosure');
      if(banner && !dismissed){ banner.style.display='flex'; }
      const btn = document.getElementById('affDismiss');
      if(btn){ btn.addEventListener('click', ()=>{ localStorage.setItem('aff_disclosure_dismissed','1'); banner.style.display='none'; }); }
    }

    // ====== CART RENDER ======
    const cartBody = document.getElementById('cartBody');
    const cartWrap = document.getElementById('cartWrap');
    const cartEmpty = document.getElementById('cartEmpty');
    const grandTotalEl = document.getElementById('grandTotal');
    const shipToEl = document.getElementById('shipTo');

    function renderCart(){
      if(!cartBody || !cartWrap || !cartEmpty || !grandTotalEl){ return 0; }
      const items = Object.entries(cart).map(([id,qty])=>{
        const p = PRODUCTS.find(x=>x.id===id);
        return {...p, qty, line:p.price*qty};
      }).filter(Boolean);
      cartBody.innerHTML = '';
      if(items.length===0){ cartWrap.style.display='none'; cartEmpty.style.display='block'; grandTotalEl.textContent='R0.00'; return 0; }
      cartWrap.style.display='block'; cartEmpty.style.display='none';
      let grand=0;
      items.forEach(item=>{
        grand += item.line;
        const tr = document.createElement('tr');
        tr.innerHTML = `
          <td>${item.name}</td>
          <td>R${item.price.toFixed(2)}</td>
          <td><input type=\"number\" min=\"1\" value=\"${item.qty}\" style=\"width:70px\"></td>
          <td class=\"right\">R${item.line.toFixed(2)}</td>
          <td class=\"right\"><button class=\"btn ghost\">Remove</button></td>`;
        tr.querySelector('input').addEventListener('change', e=>{
          const q = Math.max(1, parseInt(e.target.value||'1'));
          cart[item.id]=q; saveCart(); renderCart();
        });
        tr.querySelector('button').addEventListener('click',()=>{ delete cart[item.id]; saveCart(); renderCart(); });
        cartBody.appendChild(tr);
      });
      grandTotalEl.textContent = 'R'+grand.toFixed(2);
      return grand;
    }

    function updateCartBadge(){
      const qty = Object.values(cart).reduce((a,b)=>a+(b||0),0);
      const el = document.getElementById('navCartQty'); if(el) el.textContent = qty;
    }

    function updateNavUser(){
      const u = getUser();
      const el = document.getElementById('navAccount');
      if(!el) return;
      el.textContent = u ? 'Account' : 'Register';
    }

    function updateShipTo(){
      const u = getUser();
      if(!shipToEl) return;
      shipToEl.innerHTML = u ? `${u.first_name} ${u.last_name} — ${u.address} (☎ ${u.phone})` : 'Not set — <a href="register.html">add your address</a>';
    }

    // ====== REGISTRATION FORM HANDLERS ======
    const regForm = document.getElementById('regForm');
    const regMsg = document.getElementById('regMsg');
    if(regForm){
      // prefill if present
      const u = getUser();
      if(u){ Object.keys(u).forEach(k=>{ if(regForm[k]) regForm[k].value = u[k]; }); }
      regForm.addEventListener('submit', e=>{
        e.preventDefault();
        const data = {
          first_name: regForm.first_name.value.trim(),
          last_name:  regForm.last_name.value.trim(),
          email:      regForm.email.value.trim(),
          phone:      regForm.phone.value.trim(),
          address:    regForm.address.value.trim()
        };
        saveUser(data);
        regMsg.style.display='block';
        setTimeout(()=>regMsg.style.display='none', 3000);
      });
    }

    // ====== PAYFAST CHECKOUT ======
    // Builds a hidden form and posts to PayFast. For production, generate the signature server-side.
    function md5cycle(x, k){ /* tiny MD5 impl */
      function add32(a,b){ return (a + b) & 0xFFFFFFFF }
      function cmn(q,a,b,x,s,t){ a = add32(add32(a,q), add32(x,t)); return add32((a<<s)|(a>>> (32-s)), b) }
      function ff(a,b,c,d,x,s,t){ return cmn((b & c) | ((~b) & d),a,b,x,s,t) }
      function gg(a,b,c,d,x,s,t){ return cmn((b & d) | (c & (~d)),a,b,x,s,t) }
      function hh(a,b,c,d,x,s,t){ return cmn(b ^ c ^ d,a,b,x,s,t) }
      function ii(a,b,c,d,x,s,t){ return cmn(c ^ (b | (~d)),a,b,x,s,t) }
      var a=x[0], b=x[1], c=x[2], d=x[3];
      a=ff(a,b,c,d,k[0],7,-680876936); d=ff(d,a,b,c,k[1],12,-389564586); c=ff(c,d,a,b,k[2],17,606105819); b=ff(b,c,d,a,k[3],22,-1044525330);
      a=ff(a,b,c,d,k[4],7,-176418897); d=ff(d,a,b,c,k[5],12,1200080426); c=ff(c,d,a,b,k[6],17,-1473231341); b=ff(b,c,d,a,k[7],22,-45705983);
      a=ff(a,b,c,d,k[8],7,1770035416); d=ff(d,a,b,c,k[9],12,-1958414417); c=ff(c,d,a,b,k[10],17,-42063); b=ff(b,c,d,a,k[11],22,-1990404162);
      a=ff(a,b,c,d,k[12],7,1804603682); d=ff(d,a,b,c,k[13],12,-40341101); c=ff(c,d,a,b,k[14],17,-1502002290); b=ff(b,c,d,a,k[15],22,1236535329);
      a=gg(a,b,c,d,k[1],5,-165796510); d=gg(d,a,b,c,k[6],9,-1069501632); c=gg(c,d,a,b,k[11],14,643717713); b=gg(b,c,d,a,k[0],20,-373897302);
      a=gg(a,b,c,d,k[5],5,-701558691); d=gg(d,a,b,c,k[10],9,38016083); c=gg(c,d,a,b,k[15],14,-660478335); b=gg(b,c,d,a,k[4],20,-405537848);
      a=gg(a,b,c,d,k[9],5,568446438); d=gg(d,a,b,c,k[14],9,-1019803690); c=gg(c,d,a,b,k[3],14,-187363961); b=gg(b,c,d,a,k[8],20,1163531501);
      a=hh(a,b,c,d,k[5],4,-1444681467); d=hh(d,a,b,c,k[8],11,-51403784); c=hh(c,d,a,b,k[11],16,1735328473); b=hh(b,c,d,a,k[14],23,-1926607734);
      a=hh(a,b,c,d,k[1],4,-378558); d=hh(d,a,b,c,k[4],11,-2022574463); c=hh(c,d,a,b,k[7],16,1839030562); b=hh(b,c,d,a,k[10],23,-35309556);
      a=ii(a,b,c,d,k[0],6,-198630844); d=ii(d,a,b,c,k[7],10,1126891415); c=ii(c,d,a,b,k[14],15,-1416354905); b=ii(b,c,d,a,k[5],21,-57434055);
      a=ii(a,b,c,d,k[12],6,1700485571); d=ii(d,a,b,c,k[3],10,-1894986606); c=ii(c,d,a,b,k[10],15,-1051523); b=ii(b,c,d,a,k[1],21,-2054922799);
      x[0]=add32(a,x[0]); x[1]=add32(b,x[1]); x[2]=add32(c,x[2]); x[3]=add32(d,x[3]);
    }
    function md5blk(s){ var md5blks=[], i; for(i=0;i<64;i+=4){ md5blks[i>>2]=s.charCodeAt(i) + (s.charCodeAt(i+1)<<8) + (s.charCodeAt(i+2)<<16) + (s.charCodeAt(i+3)<<24); } return md5blks; }
    function md51(s){ var n=s.length, state=[1732584193,-271733879,-1732584194,271733878], i; for(i=64;i<=n;i+=64){ md5cycle(state, md5blk(s.substring(i-64,i))); } s=s.substring(i-64); var tail=new Array(16).fill(0); for(i=0;i<s.length;i++) tail[i>>2] |= s.charCodeAt(i) << ((i%4)<<3); tail[i>>2] |= 0x80 << ((s.length%4)<<3); if(s.length>55){ md5cycle(state, tail); tail=new Array(16).fill(0); } tail[14]=n*8; md5cycle(state, tail); return state; }
    function rhex(n){ var s='', j; for(j=0;j<4;j++) s += ('0'+((n>>>(j*8)) & 255).toString(16)).slice(-2); return s; }
    function hex(x){ for(var i=0;i<x.length;i++) x[i]=rhex(x[i]); return x.join(''); }
    function md5(s){ return hex(md51(unescape(encodeURIComponent(s)))) }

    function toUrlParams(params){
      return Object.keys(params).sort().map(k=>`${encodeURIComponent(k)}=${encodeURIComponent(params[k])}`).join('&');
    }

    function checkoutWithPayFast(){
      const total = renderCart() || 0; if(total<=0){ alert('Your cart is empty.'); return; }
      const user = getUser();
      if(!user){ if(!confirm('You have not saved delivery details yet. Continue to payment?')) return; }
      const orderId = 'SD-'+Date.now();
      const pfHost = SANDBOX ? 'https://sandbox.payfast.co.za/eng/process' : 'https://www.payfast.co.za/eng/process';
      const baseParams = {
        merchant_id: MERCHANT_ID,
        merchant_key: MERCHANT_KEY,
        return_url: RETURN_URL,
        cancel_url: CANCEL_URL,
        notify_url: NOTIFY_URL,
        amount: total.toFixed(2),
        item_name: 'SymDynamics Order '+orderId,
        m_payment_id: orderId,
        email_address: user?.email || ''
      };
      let paramString = toUrlParams(baseParams);
      // Append passphrase if provided
      let sigBase = paramString + (PASSPHRASE ? `&passphrase=${encodeURIComponent(PASSPHRASE)}` : '');
      const signature = md5(sigBase);

      // Build and submit form
      const form = document.createElement('form');
      form.action = pfHost; form.method='POST';
      Object.entries(baseParams).forEach(([k,v])=>{ const i=document.createElement('input'); i.type='hidden'; i.name=k; i.value=v; form.appendChild(i); });
      const sig=document.createElement('input'); sig.type='hidden'; sig.name='signature'; sig.value=signature; form.appendChild(sig);
      document.body.appendChild(form);
      form.submit();
    }

    const checkoutBtn = document.getElementById('checkoutBtn');
    if(checkoutBtn){ checkoutBtn.addEventListener('click', checkoutWithPayFast); }

    // year + init
    const yearEl = document.getElementById('y');
    if(yearEl){ yearEl.textContent = new Date().getFullYear(); }
    document.querySelectorAll('a[href^="#"]').forEach(a=>a.addEventListener('click',e=>{ const id=a.getAttribute('href'); const el=document.querySelector(id); if(el){ e.preventDefault(); el.scrollIntoView({behavior:'smooth'});} }));
    renderCart(); updateCartBadge(); updateNavUser(); updateShipTo();
});