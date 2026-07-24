from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import QSizePolicy, QWidget

from .effects.aurora_field import COLOR_1, COLOR_2, lerp_rgb
from .led_geometry import LED_COUNT
from .model import RGB


class AuroraFieldChart(QWidget):
    """LED-by-LED view of the Aurora field's brightness and color progress."""

    _LEFT_MARGIN = 5
    _RIGHT_MARGIN = 0
    _TOP_MARGIN = 14
    _BOTTOM_MARGIN = 52
    _POINT_SPACING = 28
    _POINT_RADIUS = 5

    def __init__(self) -> None:
        super().__init__()
        self.brightness = (0,) * LED_COUNT
        self.color_progress = (0,) * LED_COUNT
        self.color_1 = COLOR_1
        self.color_2 = COLOR_2
        self.setMinimumSize(
            self._LEFT_MARGIN
            + self._RIGHT_MARGIN
            + (LED_COUNT - 1) * self._POINT_SPACING,
            230,
        )
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.MinimumExpanding)

    def set_field(
        self,
        brightness: tuple[int, ...],
        color_progress: tuple[int, ...],
        color_1: RGB,
        color_2: RGB,
    ) -> None:
        if len(brightness) != len(color_progress):
            raise ValueError("Brightness and color progress sizes differ")

        self.brightness = tuple(max(0, min(255, int(value))) for value in brightness)
        self.color_progress = tuple(
            max(0, min(255, int(value))) for value in color_progress
        )
        self.color_1 = color_1
        self.color_2 = color_2

        required_width = (
            self._LEFT_MARGIN
            + self._RIGHT_MARGIN
            + max(0, len(self.brightness) - 1) * self._POINT_SPACING
        )
        self.setMinimumWidth(required_width)
        self.update()

    def point_color(self, index: int) -> RGB:
        return lerp_rgb(
            self.color_1,
            self.color_2,
            self.color_progress[index],
        )

    def paintEvent(self, _) -> None:
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        painter.fillRect(self.rect(), QColor(12, 14, 18))

        plot_top = self._TOP_MARGIN
        plot_bottom = max(plot_top + 1, self.height() - self._BOTTOM_MARGIN)
        plot_height = plot_bottom - plot_top
        plot_right = max(
            self._LEFT_MARGIN,
            self.width() - self._RIGHT_MARGIN,
        )

        small_font = QFont(painter.font())
        small_font.setPointSize(7)
        painter.setFont(small_font)

        for value in (0, 64, 128, 192, 255):
            y = plot_bottom - value * plot_height / 255
            painter.setPen(QPen(QColor(48, 53, 62), 1))
            painter.drawLine(
                QPointF(self._LEFT_MARGIN, y),
                QPointF(plot_right, y),
            )
            painter.setPen(QColor(139, 145, 156))
            painter.drawText(
                QRectF(0, y - 8, self._LEFT_MARGIN - 5, 16),
                Qt.AlignRight | Qt.AlignVCenter,
                str(value),
            )

        if not self.brightness:
            return

        points = [
            QPointF(
                self._LEFT_MARGIN + index * self._POINT_SPACING,
                plot_bottom - value * plot_height / 255,
            )
            for index, value in enumerate(self.brightness)
        ]

        painter.setPen(QPen(QColor(93, 101, 115), 1))
        for start, end in zip(points, points[1:]):
            painter.drawLine(start, end)

        value_width = self._POINT_SPACING
        for index, point in enumerate(points):
            color = QColor(*self.point_color(index))
            painter.setPen(QPen(color.lighter(130), 1))
            painter.setBrush(color)
            painter.drawEllipse(
                point,
                self._POINT_RADIUS,
                self._POINT_RADIUS,
            )

            text_left = point.x() - value_width / 2
            painter.setPen(QColor(224, 227, 232))
            painter.drawText(
                QRectF(text_left, plot_bottom + 4, value_width, 17),
                Qt.AlignHCenter | Qt.AlignVCenter,
                f"{self.brightness[index]}",
            )
            painter.setPen(color.lighter(135))
            painter.drawText(
                QRectF(text_left, plot_bottom + 21, value_width, 17),
                Qt.AlignHCenter | Qt.AlignVCenter,
                f"{self.color_progress[index]}",
            )
