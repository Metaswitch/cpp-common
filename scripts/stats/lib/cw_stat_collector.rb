#!/usr/bin/env ruby

# @file cw_stat_collector.rb
#
# Copyright (C) Metaswitch Networks 2017
# If license terms are provided to you in a COPYING file in the root directory
# of the source code repository by which you are accessing this code, then
# the license outlined in that COPYING file applies to your use.
# Otherwise no rights are granted except for those provided to you by
# Metaswitch Networks in a separate written agreement.

require 'ffi-rzmq'
require 'optparse'
require 'timeout'

# This class creates and owns a subscription to a statistic exposed
# by a clearwater component.
#
# The subscription is created at class initialization time but the
# values will not be read until the run method is called.
#
# The subscription can be created in single-shot mode, where the value of the
# statistic will be polled once only or in repeated mode, where the collector
# will loop, reporting changes to the statistic when they happen.
class CWStatCollector
  @@known_stats = {}

  # Registers a renderer to use for a given statistic.  If a rendered is not
  # specified, the statistic cannot be queried
  #
  # For most cases, SimpleStatRenderer will be sufficient.
  def self.register_stat(statname, renderer, options={})
    @@known_stats[statname] = renderer
  end

  # Create a collector for each known statistic.
  #
  # @param options Two options are supported:
  #   - :verbose to add more logging
  #   - :subscribe to use repeated mode
  def self.all_collectors(hostname, port, options)
    @@known_stats.keys.map do |statname|
      CWStatCollector.new(hostname, port, statname, options)
    end
  end

  # Create a StatCollector and start the subscription.  This will throw
  # an exception if the requested statistic is not recognized.
  #
  # @param options Two options are supported:
  #   - :verbose to add more logging
  #   - :subscribe to use repeated mode
  def initialize(hostname, port, statname, options)
    fail "Statistic \"#{statname}\" not recognised" if not @@known_stats.include? statname
    renderer = @@known_stats[statname]

    @verbose = options[:verbose]
    @subscribe = options[:subscribe]
    @statname = statname
    @stat_renderer = renderer.new
    log "Creating subscription for #{statname} from #{hostname}"
    log "Stats will be rendered using #{@stat_renderer.class}"
    @context = ZMQ::Context.new
    @socket = @context.socket(ZMQ::SUB)
    @socket.connect("tcp://#{hostname}:#{port}")
    @socket.setsockopt(ZMQ::SUBSCRIBE, statname)
    @latest_status = {}
    @latest_stats = {}
  end

  # Start the processing to retrieve the value for the collected
  # statistic.  If the collector is in subscribe mode, this function
  # loops forever.  If we don't get our first response within 5
  # seconds, assume the remote node is down and bail out.
  #
  # If a block is given, the rendered statistic will be yielded to the
  # block.  Otherwise, the rendered statistic will be written to
  # STDOUT. If the statistic does not exist (i.e., the subscription
  # does not return OK), the block is not evaluated / nothing is
  # written to STDOUT.
  def run &blk
    begin
      Timeout::timeout(5) do
        get_stat &blk
      end
    rescue Timeout::Error => e
      puts "Error: No response from host, ensure hostname and port are correct"
      throw e
    rescue Exception => e
      puts "Error: Unexpected exception occurred: #{e}"
      puts e.backtrace
      throw e
    end

    if @subscribe
      loop { get_stat &blk }
    end
  end

  private

  def log msg
    puts "DEBUG: #{msg}" if @verbose
  end

  def read_one_entry
    log "Waiting for next entry"
    @socket.recv_strings(msg = [])
    log "Received entry: #{msg[0]} (#{msg[1]}) => #{msg[2..-1]}"
    @latest_status[msg[0]] = msg[1]
    @latest_stats[msg[0]] = msg[2..-1]
    msg[0]
  end

  def get_stat
    loop do
      topic = read_one_entry
      break if topic == @statname
    end
    if @latest_status[@statname] == "OK"
      rendered_response = @stat_renderer.render(@latest_stats[@statname])
      if block_given?
        yield rendered_response
      else
        puts "#{@statname}:\n#{rendered_response}"
      end
    else
      # Just swallow unknown statistics.
    end
  end
end

# A base class for stat renderers.
# A subclass must implement the render method.
#
# @abstract
class AbstractRenderer
  # Render a message (as an array of strings) into an
  # appropriate format for further processing.
  def render msg
    fail "Abstract renderer should never be used directly"
  end
end

# This renderer reports call statistics.
#
# The output format is picked to be human readable, while still easy to
# parse so it can be given to another tool (cacti for example).
class CallStatsRenderer < AbstractRenderer
  # @see AbstractRenderer#render
  def render(msg)
    <<-EOF
initial_registers:#{msg[0]}
initial_registers_delta:#{msg[4]}
ongoing_registers:#{msg[1]}
ongoing_registers_delta:#{msg[5]}
call_attempts:#{msg[2]}
call_attempts_delta:#{msg[6]}
successful_calls:#{msg[3]}
successful_calls_delta:#{msg[7]}
    EOF
  end
end

# This simple renderer assumes that there is only one section in the
# reported statistic and extracts it.
#
# This will be sufficient for many statistics.
class SimpleStatRenderer < AbstractRenderer
  # @see AbstractRenderer#render
  def render(msg)
    msg[0] or "No value returned"
  end
end

# The connected_sprouts (etc) statistic is reported as:
#
# <ip_address1>
#
# <connection_count1>
#
# <ip_address2>
#
# <connection_count2>
#
# <ip_address3>
#
# <connection_count3>
#
# ...
#
# We convert it into a hash of ip_address => connection_count
class ConnectedIpsRenderer < AbstractRenderer
  # @see AbstractRenderer#render
  def render(msg)
    hash = {}
    while not msg.empty?
      k = msg.shift
      v = msg.shift
      hash[k] = v.to_i
    end
    hash
  end
end

# This renders latency statistics, and provides a count of how many events
# occured
class LatencyCountStatsRenderer < AbstractRenderer
  # @see AbstractRenderer#render
  def render(msg)
    <<-EOF
mean:#{msg[0]}
variance:#{msg[1]}
lwm:#{msg[2]}
hwm:#{msg[3]}
count:#{msg[4]}
    EOF
  end
end

# Register the sprout/bono/sipp stats.
CWStatCollector.register_stat("client_count", SimpleStatRenderer)
CWStatCollector.register_stat("connected_homesteads", ConnectedIpsRenderer)
CWStatCollector.register_stat("connected_homers", ConnectedIpsRenderer)
CWStatCollector.register_stat("connected_sprouts", ConnectedIpsRenderer)
CWStatCollector.register_stat("call_stats", CallStatsRenderer)
CWStatCollector.register_stat("latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("hss_latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("hss_digest_latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("hss_subscription_latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("hss_user_auth_latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("hss_location_latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("xdm_latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("incoming_requests", SimpleStatRenderer)
CWStatCollector.register_stat("rejected_overload", SimpleStatRenderer)
CWStatCollector.register_stat("queue_size", LatencyCountStatsRenderer)

# Register for homer/homestead stats.
CWStatCollector.register_stat("H_latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("H_hss_latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("H_hss_digest_latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("H_hss_subscription_latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("H_cache_latency_us", LatencyCountStatsRenderer)
CWStatCollector.register_stat("H_incoming_requests", SimpleStatRenderer)
CWStatCollector.register_stat("H_rejected_overload", SimpleStatRenderer)

# Register the homestead-prov stats.
# This currently only listens for stats for the first process.
CWStatCollector.register_stat("P_latency_us_0", LatencyCountStatsRenderer)
CWStatCollector.register_stat("P_queue_size_0", LatencyCountStatsRenderer)
CWStatCollector.register_stat("P_incoming_requests_0", SimpleStatRenderer)
CWStatCollector.register_stat("P_rejected_overload_0", SimpleStatRenderer)
