#!/usr/bin/ruby

require 'json'
require 'typhoeus'
require 'pty'

module WeatherMon
  class Reader
    def initialize
      @cmd = '/root/weathermon/weathermon'
    end

    def read_data
      PTY.spawn @cmd do |r, _w, pid|
        begin
          r.sync
          r.each_line do |l|
            time = Time.now
            puts "#{time.strftime('%H:%M:%S')} - #{l.strip}"
            data = parse_data(l.strip)
            send_data(data)
          end
        rescue Errno::EIO => e
          # simply ignoring this
        ensure
          ::Process.wait pid
        end
      end
    end

    private

    def parse_data data
      a = JSON.parse(data, symbolize_names: true)
      a[:temp] = (a[:temp]/10.0 - 32) * (5.0/9.0)
      a
    end

    def send_data data
      Typhoeus.post(
        'localhost/thermometer_temperatures.json',
        headers: {
          "Content-Type" => "application/json"
        },
        body: JSON.generate(
          {
            thermometer_id: data[:id],
            temperature: data[:temp],
            humidity: data[:humidity]
          }
        )
      )
    end
  end
end

WeatherMon::Reader.new.read_data
