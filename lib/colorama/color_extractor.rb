# frozen_string_literal: true

module Colorama::ColorExtractor
  THRESHOLD_MODIFIER = 0.01

  class << self
    def most_used_colors(image, quality)
      resized_image = pre_process_image(image, quality)

      height = resized_image.rows

      threshold = (height * THRESHOLD_MODIFIER).to_i
      proposed = [-1, -1, -1, -1]

      image_colors = {}

      resized_image.each_pixel do |pixel, _c, _r|
        next if pixel.alpha < 127

        red = pixel.red & 255
        green = pixel.green & 255
        blue = pixel.blue & 255

        index = red.to_i * 1_000_000 + green * 1_000 + blue

        image_colors[index] = 0 unless image_colors.include?(index)

        image_colors[index] += 1
      end

      image_colors.sort_by { |_key, value| -value }

      sorted_colors = []

      image_colors.each do |key, value|
        sorted_colors << { color: Colorama::Color.new_from_double(key), count: value } if threshold < value
      end

      proposed[0] = extract_edge_color(sorted_colors)

      sorted_colors.clear

      first_color_dark = !proposed[0].dark_color?

      image_colors.each do |key, value|
        color = Colorama::Color.new_from_double(key).with(0.15)

        sorted_colors << { color: color, count: value } if color.dark_color? == first_color_dark
      end

      sorted_colors.each do |entry|
        color = entry[:color]

        if proposed[1] == -1
          proposed[1] = color if color.contrasting?(proposed[0])
        elsif proposed[2] == -1
          next if !color.contrasting?(proposed[0]) || !proposed[1].distinct?(color)

          proposed[2] = color
        elsif proposed[3] == -1
          next if !color.contrasting?(proposed[0]) || !proposed[2].distinct?(color) || !proposed[1].distinct?(color)

          proposed[3] = color
          break
        end
      end

      dark_background = proposed[0].dark_color?

      (1..3).each do |i|
        proposed[i] = Colorama::Color.new_from_double(dark_background ? 255_255_255 : 0) if proposed[i] == -1
      end

      {
        background: proposed[0],
        primary: proposed[1],
        secondary: proposed[2],
        detail: proposed[3]
      }
    end

    private

    def pre_process_image(image, quality)
      resize_size = sanitize_quality(quality)

      resize_size.positive? ? scale_image(image, resize_size, resize_size) : image
    end

    def extract_edge_color(sorted_colors)
      proposed_edge_color = { color: Colorama::Color.new_from_double(0), count: 1 }

      proposed_edge_color = sorted_colors.first if sorted_colors.count.positive?

      if proposed_edge_color[:color].black_or_white? && sorted_colors.count > 1
        (1..sorted_colors.count).each do |i|
          next_proposed_color = sorted_colors[i]
          if (next_proposed_color[:count] / proposed_edge_color[:count]) > 0.3
            unless next_proposed_color[:color].black_or_white?
              proposed_edge_color = next_proposed_color
              break
            end
          else
            break
          end
        end
      end

      proposed_edge_color[:color]
    end

    def scale_image(image, max_width = 512, max_height = 512)
      min_width = [image.columns, max_width].min
      min_height = [image.rows, max_height].min

      image.resize_to_fit(min_width, min_height)
    end

    def sanitize_quality(quality)
      {
        lowest: 50,
        low: 100,
        high: 250
      }[quality] || 0
    end
  end
end
