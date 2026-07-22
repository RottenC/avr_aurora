from PySide6.QtCore import Qt
from PySide6.QtGui import QColor, QPainter, QPen
from PySide6.QtWidgets import QWidget
from .led_geometry import physical_coordinates, LED_COUNT

class LedCanvas(QWidget):
    def __init__(self, physical=False): super().__init__(); self.physical=physical; self.leds=[(0,0,0)]*LED_COUNT; self.show_indices=False; self.setMinimumHeight(130 if not physical else 280)
    def set_leds(self, leds): self.leds=list(leds); self.update()
    def paintEvent(self, _):
        p=QPainter(self); p.fillRect(self.rect(), Qt.black); p.setPen(QPen(Qt.gray))
        if self.physical:
            coords=physical_coordinates(); scale=min((self.width()-40)/12,(self.height()-40)/25)
            pts=[(20+x*scale,20+y*scale) for x,y in coords]
        else:
            gap=max(10,(self.width()-20)/LED_COUNT); pts=[(10+i*gap,self.height()/2) for i in range(LED_COUNT)]
        for i,(x,y) in enumerate(pts):
            p.setBrush(QColor(*self.leds[i])); p.drawEllipse(int(x)-5,int(y)-5,10,10)
            if self.show_indices: p.drawText(int(x)-8,int(y)-9,str(i))
