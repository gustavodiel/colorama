class Colorama::Color
  attr_accessor :red, :green, :blue

  def self.new_from_rgb(r, g, b)
    new(r, g, b)
  end

  def self.new_from_double(number)
    r = ((number / 1_000_000).floor % 1_000_000)
    g = ((number / 1_000).floor % 1_000)
    b = (number % 1_000)

    new(r, g, b)
  end

  def initialize(r, g, b)
    @red = r
    @green = g
    @blue = b
  end

  def hex
    red_hex = red.to_s(16)
    green_hex = green.to_s(16)
    blue_hex = blue.to_s(16)

    red_hex += red_hex unless red_hex.length == 2
    green_hex += green_hex unless green_hex.length == 2
    blue_hex += blue_hex unless blue_hex.length == 2

    "#{red_hex}#{green_hex}#{blue_hex}"
  end

  # This algorithm is based on our own vision.
  # The human eye detects more green then red and yellow,
  # this is the reason of those numbers
  def dark_color?
    (luminance < 127.5)
  end

  def black_or_white?
    ((red > 232 && green > 232 && blue > 232) || (red < 23 && green < 23 && blue < 23))
  end

  def distinct?(other_color)
    (
      (red - other_color.red).abs > 63.75 ||
        (green - other_color.green).abs > 63.75 ||
        (blue - other_color.blue).abs > 63.75
    ) && !(
      (red - green).abs < 7.65 &&
        (red - blue).abs < 7.65 &&
        (other_color.red - other_color.green).abs < 7.65 &&
        (other_color.red - other_color.blue).abs < 7.65
    )
  end

  def luminance
    (red * 0.2126) + (green * 0.7152) + (blue * 0.0722)
  end

  def with(min_saturation)
    _r = red / 255.0
    _g = green / 255.0
    _b = blue / 255.0

    m = [_r, _g, _b].max
    c = m - [_r, _g, _b].min

    v = m
    s = (v.zero? && 0 || (c / v))

    return self if min_saturation <= s

    h = if _r == m
          (((_g - _b) / c) % 6)
        elsif _g == m
          (2 + ((_g - _r) / c))
        else
          (4 + ((_r - _g) / c))
        end

    h += 6 if h.negative?

    # Return to RGB

    c = v * min_saturation
    x = (c * (1 - (h % 2).abs - 1))

    case h
      when 0...1
        new_r = c
        new_g = x
        new_b = 0
      when 1...2
        new_r = x
        new_g = c
        new_b = 0
      when 2...3
        new_r = 0
        new_g = c
        new_b = x
      when 3...4
        new_r = 0
        new_g = x
        new_b = c
      when 4...5
        new_r = x
        new_g = 0
        new_b = c
      when 5...6
        new_r = c
        new_g = 0
        new_b = x
      else
        new_r = 0
        new_g = 0
        new_b = 0
    end

    m = v - c

    number = ((((new_r + m) * 255).floor * 1_000_000) + (((new_g + m) * 255).floor * 1_000) + ((new_b + m) * 255).floor)

    Colorama::Color.new_from_double(number)
  end

  def contrasting?(other_color)
    background_luminance = luminance + 12.75
    foreground_luminance = other_color.luminance + 12.75

    return ((background_luminance / foreground_luminance) > 1.6) if background_luminance > foreground_luminance

    (foreground_luminance / background_luminance) > 1.6
  end

  def eql?(other)
    other_hex = other.is_a?(self.class) ? other.hex : other

    other_hex == hex
  rescue StandardError => e
    false
  end

  def ==(other)
    eql?(other)
  end
end
