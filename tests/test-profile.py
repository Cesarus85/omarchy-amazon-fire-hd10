#!/usr/bin/env python3
from pathlib import Path
import json,sys,tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from device_profile import load_profile,render
good={'root_uuid':'11111111-2222-4333-8444-555555555555','root_bytes':31058804224,
 'boot_sha256':'1'*64,'recovery_sha256':'2'*64,'sd_uuid':'NO-SD-CARD'}
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'profile.json'
 def check(data):p.write_text(json.dumps(data));return load_profile(p)
 assert render('@ROOT_UUID@ / @ROOT_BYTES@',check(good))==good['root_uuid']+' / '+str(good['root_bytes'])
 failures=[
  {**good,'root_uuid':"bad'; shell"},
  {**good,'root_bytes':True},{**good,'root_bytes':31058804225},
  {**good,'root_bytes':0},{**good,'boot_sha256':'0'*64},
  {**good,'recovery_sha256':'REPLACE_ME'},{**good,'sd_uuid':'../../other'},
  {**good,'sd_uuid':'x\nExecStart=bad'},{**good,'unexpected':'value'},
 ]
 for bad in failures:
  try:check(bad)
  except (ValueError,TypeError,AttributeError):pass
  else:raise AssertionError('Invalid profile accepted')
 try:render('@MISSING@',check(good))
 except ValueError:pass
 else:raise AssertionError('Unresolved token accepted')
print('PASS: valid rendering and 10 malformed/unresolved input cases')
