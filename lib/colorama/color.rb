# frozen_string_literal: true

module Colorama
  class Color
    attr_reader :red, :green, :blue

    WHITE_THRESHOLD = 232
    BLACK_THRESHOLD = 23

    def self.new_from_rgb(r, g, b)
      new(r, g, b)
    end

    def self.new_from_double(number)
      r = ((number / 1_000_000).floor % 1_000_000)
      g = ((number / 1_000).floor % 1_000)
      b = (number % 1_000)

      new(r, g, b)
    end

    def initialize(red, green, blue)
      @red = red
      @green = green
      @blue = blue
    end

    def hex
      @hex ||= begin
        red_hex = red.to_s(16).rjust(2, '0')
        green_hex = green.to_s(16).rjust(2, '0')
        blue_hex = blue.to_s(16).rjust(2, '0')

        "#{red_hex}#{green_hex}#{blue_hex}"
      end
    end

    def dark_color?
      luminance < 127.5
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

    # This algorithm is based on our own vision.
    # The human eye detects more green then red and yellow,
    # this is the reason of those numbers
    def luminance
      @luminance ||= red * 0.2126 + green * 0.7152 + blue * 0.0722
    end

    def with(min_saturation)
      normalized_red = red / 255.0
      normalized_green = green / 255.0
      normalized_blue = blue / 255.0

      normalized_max = [normalized_red, normalized_green, normalized_blue].max
      normalized_min = normalized_max - [normalized_red, normalized_green, normalized_blue].min

      s = normalized_max.zero? ? 0 : (normalized_min / normalized_max)

      return self if min_saturation <= s

      hue = if normalized_min.zero?
              0
            elsif normalized_red == normalized_max
              ((normalized_green - normalized_blue) / normalized_min) % 6
            elsif normalized_green == normalized_max
              2 + ((normalized_green - normalized_red) / normalized_min)
            else
              4 + ((normalized_red - normalized_green) / normalized_min)
            end

      hue += 6 if hue.negative?

      normalized_min = normalized_max * min_saturation
      x = normalized_min * (1 - (hue % 2).abs - 1)

      case hue
      when 0...1
        new_r = normalized_min
        new_g = x
        new_b = 0
      when 1...2
        new_r = x
        new_g = normalized_min
        new_b = 0
      when 2...3
        new_r = 0
        new_g = normalized_min
        new_b = x
      when 3...4
        new_r = 0
        new_g = x
        new_b = normalized_min
      when 4...5
        new_r = x
        new_g = 0
        new_b = normalized_min
      when 5...6
        new_r = normalized_min
        new_g = 0
        new_b = x
      else
        new_r = 0
        new_g = 0
        new_b = 0
      end

      normalized_max -= normalized_min

      Colorama::Color.new_from_double(
        (((new_r + normalized_max) * 255).floor * 1_000_000) +
        (((new_g + normalized_max) * 255).floor * 1_000) +
        ((new_b + normalized_max) * 255).floor
      )
    end

    def contrasting?(other_color)
      background_luminance = luminance + 12.75
      foreground_luminance = other_color.luminance + 12.75

      return ((background_luminance / foreground_luminance) > 1.6) if background_luminance > foreground_luminance

      (foreground_luminance / background_luminance) > 1.6
    end

    def white?
      red > WHITE_THRESHOLD && green > WHITE_THRESHOLD && blue > WHITE_THRESHOLD
    end

    def black?
      red < BLACK_THRESHOLD && green < BLACK_THRESHOLD && blue < BLACK_THRESHOLD
    end

    def black_or_white?
      black? || white?
    end

    def eql?(other)
      other_hex = other.is_a?(self.class) ? other.hex : other

      other_hex == hex
    rescue StandardError
      false
    end

    def ==(other)
      eql?(other)
    end
  end
end
