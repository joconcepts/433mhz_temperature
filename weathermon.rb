#!/usr/bin/ruby2.1

require 'json'
require 'sqlite3'
require 'pty'

module WeatherMon
  class Reader
    def initialize
      @db = SQLite3::Database.new("/root/weathermon/weathermon.db")
      @cmd = '/root/weathermon/weathermon'
      create_table
    end

    def read_data
      PTY.spawn @cmd do |r, _w, pid|
        begin
          r.sync
          r.each_line do |l|
            time = Time.now
            puts "#{time.strftime('%H:%M:%S')} - #{l.strip}"
            data = parse_data(l.strip)
            write_db(time.to_i, data)
          end
        rescue Errno::EIO => e
          # simply ignoring this
        ensure
          ::Process.wait pid
        end
      end
    end

    private

    def create_table
      @db.execute <<-SQL
        CREATE TABLE IF NOT EXISTS sensors (
          timestamp int primary key,
          sensor_id int,
          temp real,
          humidity int,
          battery int
        );
      SQL
    end

    def parse_data data
      a = JSON.parse(data, symbolize_names: true)
      a[:temp] = (a[:temp]/10.0 - 32) * (5.0/9.0)
      a
    end

    def write_db time, data
      query = <<-SQL
        INSERT INTO sensors(
          timestamp, sensor_id, temp, humidity, battery
        ) VALUES (?, ?, ?, ?, ?);
      SQL
      @db.execute(query, [time, data[:id], data[:temp], data[:humidity], data[:battery]])
    end
  end
end

WeatherMon::Reader.new.read_data
