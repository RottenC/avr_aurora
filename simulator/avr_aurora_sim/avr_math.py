from .diagnostics import Diagnostics

def _diag(diag, op, inputs, result, label="", strict=False):
    if diag: diag.record(op, inputs, result, label, strict)

def u8(value:int, diagnostics:Diagnostics|None=None, label:str="")->int:
    r=value & 0xFF
    if r!=value: _diag(diagnostics,"u8_wrap",(value,),r,label)
    return r

def u16(value:int, diagnostics=None, label:str="")->int:
    r=value & 0xFFFF
    if r!=value: _diag(diagnostics,"u16_wrap",(value,),r,label)
    return r

def s8(value:int, diagnostics=None, label:str="")->int:
    raw=value & 0xFF; r=raw-256 if raw & 0x80 else raw
    if r!=value: _diag(diagnostics,"signed_wrap",(value,),r,label)
    return r

def s16(value:int, diagnostics=None, label:str="")->int:
    raw=value & 0xFFFF; r=raw-65536 if raw & 0x8000 else raw
    if r!=value: _diag(diagnostics,"signed_wrap",(value,),r,label)
    return r

def qadd8(a:int,b:int,diagnostics=None,label:str="")->int:
    total=a+b; r=255 if total>255 else total
    if total>255: _diag(diagnostics,"saturating_add",(a,b),r,label)
    return r

def qsub8(a:int,b:int,diagnostics=None,label:str="")->int:
    r=0 if b>a else a-b
    if b>a: _diag(diagnostics,"saturating_sub",(a,b),r,label)
    return r

def clamp_u8(value:int,diagnostics=None,label:str="")->int:
    r=max(0,min(255,value))
    if r!=value: _diag(diagnostics,"clamped_value",(value,),r,label,True)
    return r

def clamp_u16(value:int,diagnostics=None,label:str="")->int:
    r=max(0,min(65535,value))
    if r!=value: _diag(diagnostics,"clamped_value",(value,),r,label,True)
    return r

def div_trunc(n:int,d:int,diagnostics=None,label:str="")->int:
    if d==0: raise ZeroDivisionError(label or "div_trunc")
    q=abs(n)//abs(d); r=-q if (n<0)^(d<0) else q
    if n % d: _diag(diagnostics,"truncating_division",(n,d),r,label)
    return r

def scale8(value:int,scale:int,diagnostics=None,label:str="")->int:
    return u8(div_trunc(u16(value)*u16(scale),256,diagnostics,label or "scale8"),diagnostics,label)

def lerp8(a:int,b:int,amount:int,diagnostics=None,label:str="")->int:
    delta=b-a
    return clamp_u8(a + div_trunc(delta*amount,255,diagnostics,label or "lerp8"),diagnostics,label)
