# frozen_string_literal: true

module Colorama
  module ColorExtractor
    THRESHOLD_MODIFIER = 0.01

    class << self
      def most_used_colors(image, quality)
        resize_size = sanitize_quality(quality)

        resized_image = resize_size.positive? ? scale_image(image, resize_size, resize_size) : image

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

        proposed[0] = proposed_edge_color[:color]

        sorted_colors.clear

        find_dark_text_color = !proposed[0].dark_color?

        image_colors.each do |key, value|
          color = Colorama::Color.new_from_double(key)
          k = color.with(0.15)
          sorted_colors << { color: k, count: value } if k.dark_color? == find_dark_text_color
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

        proposed.count.times.each do |i|
          proposed[i] = Colorama::Color.new_from_double(dark_background && 255_255_255 || 0) if proposed[i] == -1
        end

        {
          background_color: proposed[0],
          primary_color: proposed[1],
          secondary_color: proposed[2],
          detail_color: proposed[3]
        }
      end

      def most_used_colors_histogram(image, color_depth)
        quantified = image.quantize(color_depth, Magick::RGBColorspace)

        palette = quantified.color_histogram.sort { |a, b| b[1] <=> a[1] }

        [].tap do |array|
          palette.each do |p|
            palette_color = p.first
            color = Colorama::Color.new_from_rgb(palette_color.red, palette_color.green, palette_color.blue)

            array << color.hex
          end
        end
      end

      private

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
end
