# SPDX-License-Identifier: GPL-2.0-only
"""Validate explicit per-device build inputs; never discover or modify a device."""
import json,re,uuid
from pathlib import Path
def load_profile(path):
 p=json.loads(Path(path).read_text())
 required={'root_uuid','root_bytes','boot_sha256','recovery_sha256','sd_uuid'}
 if set(p)!=required:raise ValueError('Profile fields must be exactly '+', '.join(sorted(required)))
 p['root_uuid']=str(uuid.UUID(p['root_uuid']))
 if type(p['root_bytes']) is not int or not 4*1024**3 <= p['root_bytes'] < 128*1024**3 or p['root_bytes']%512:
  raise ValueError('root_bytes must be the verified, sector-aligned partition size')
 for k in ('boot_sha256','recovery_sha256'):
  if not isinstance(p[k],str) or not re.fullmatch('[0-9a-f]{64}',p[k]) or p[k]=='0'*64:
   raise ValueError(k+' must be the measured SHA256, not an example value')
 if not isinstance(p['sd_uuid'],str) or not re.fullmatch('[A-Za-z0-9-]{1,64}',p['sd_uuid']):
  raise ValueError('sd_uuid must be a filesystem UUID or NO-SD-CARD')
 return {k.upper():str(v) for k,v in p.items()}
def render(text,profile):
 for key,value in profile.items():text=text.replace('@'+key+'@',value)
 if re.search(r'@[A-Z_]+@',text):raise ValueError('Unresolved template token')
 return text
