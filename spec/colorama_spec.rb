# frozen_string_literal: true

require "rmagick"

require_relative "../lib/colorama"

RSpec.describe Colorama do
  describe '.extract_from_file' do
    subject(:colors) { described_class.extract_from_file(filename, quality: quality) }
    let(:quality) { :highest }

    shared_context 'a color extractor' do
      it 'returns the right color' do
        expect(colors).to eq(
          background: expected_background,
          detail: expected_detail,
          primary: expected_primary,
          secondary: expected_secondary,
        )
      end
    end

    context 'on baloon party album' do
      let(:filename) { 'spec/fixtures/balloon_party.jpg' }
      let(:expected_background) { '000000' }
      let(:expected_detail) { 'efcba4' }
      let(:expected_primary) { '8f8eae' }
      let(:expected_secondary) { 'd2a6f8' }

      it_behaves_like 'a color extractor'
    end

    context 'on Deadmau5' do
      let(:filename) { 'spec/fixtures/deadmau5.jpg' }
      let(:expected_background) { '222220' }
      let(:expected_detail) { 'a38a8a' }
      let(:expected_primary) { '4d9d3e' }
      let(:expected_secondary) { '85de72' }

      it_behaves_like 'a color extractor'
    end

    context 'on Deadmau5 2' do
      let(:filename) { 'spec/fixtures/deadmau5_2.jpg' }
      let(:expected_background) { '1a0033' }
      let(:expected_detail) { 'febcc6' }
      let(:expected_primary) { 'be6f8e' }
      let(:expected_secondary) { 'ff6ca9' }

      it_behaves_like 'a color extractor'
    end

    context 'on Drake' do
      let(:filename) { 'spec/fixtures/drake.jpg' }
      let(:expected_background) { '4b9fdd' }
      let(:expected_detail) { '001027' }
      let(:expected_primary) { '0066b7' }
      let(:expected_secondary) { '994076' }

      it_behaves_like 'a color extractor'
    end

    context 'on Knife Party' do
      let(:filename) { 'spec/fixtures/knife_party.jpg' }
      let(:expected_background) { '171717' }
      let(:expected_detail) { 'ffffff' }
      let(:expected_primary) { '998282' }
      let(:expected_secondary) { 'f1cccc' }

      it_behaves_like 'a color extractor'
    end

    context 'on A Little Piece of Heaven' do
      let(:filename) { 'spec/fixtures/little_piece_of_heaven.jpg' }
      let(:expected_background) { 'fdfdfd' }
      let(:expected_detail) { '000000' }
      let(:expected_primary) { '7e6b6b' }
      let(:expected_secondary) { '886666' }

      it_behaves_like 'a color extractor'
    end

    context 'on purple haze' do
      let(:filename) { 'spec/fixtures/purple_haze.jpg' }
      let(:expected_background) { 'b0b9d8' }
      let(:expected_detail) { '49ff49' }
      let(:expected_primary) { '6f5787' }
      let(:expected_secondary) { 'b13a7c' }

      it_behaves_like 'a color extractor'
    end

    context 'on RAM' do
      let(:filename) { 'spec/fixtures/ram.jpg' }
      let(:expected_background) { 'cceedd' }
      let(:expected_detail) { 'd8b7b7' }
      let(:expected_primary) { '9177ab' }
      let(:expected_secondary) { 'd1acf7' }

      it_behaves_like 'a color extractor'
    end
  end
end
