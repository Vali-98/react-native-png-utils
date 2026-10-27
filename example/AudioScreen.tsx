import React, { useRef, useState } from 'react';
import {
  ActivityIndicator,
  ScrollView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';
import {
  errorCodes,
  isErrorWithCode,
  keepLocalCopy,
  pick,
} from '@react-native-documents/picker';
import { Dirs, FileSystem } from 'react-native-file-access';
import { convertToWav } from '../src';
import type { WavConversionResult } from '../src';
import { audioFixtures } from './audioFixtures';

type Source = {
  path: string;
  label: string;
  expected?: { sampleRate: number; channels: number; durationSeconds: number };
};
function Action({
  title,
  disabled,
  onPress,
}: {
  title: string;
  disabled: boolean;
  onPress: () => void;
}) {
  return (
    <TouchableOpacity
      accessibilityRole="button"
      disabled={disabled}
      onPress={onPress}
      style={[styles.button, disabled && styles.disabled]}
    >
      <Text style={styles.buttonText}>{title}</Text>
    </TouchableOpacity>
  );
}
export default function AudioScreen() {
  const [source, setSource] = useState<Source | null>(null);
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState(
    'Choose a sample or a local media file.',
  );
  const [result, setResult] = useState<WavConversionResult | null>(null);
  const [elapsed, setElapsed] = useState(0);
  const [outputPath, setOutputPath] = useState<string | null>(null);
  const working = useRef(false);
  const start = () => {
    if (working.current) return false;
    working.current = true;
    setBusy(true);
    return true;
  };
  const finish = () => {
    working.current = false;
    setBusy(false);
  };
  const error = (value: unknown) =>
    setStatus(value instanceof Error ? value.message : String(value));
  const directory = async () => {
    const path = `${Dirs.CacheDir}/png-utils-audio`;
    if (!(await FileSystem.exists(path))) await FileSystem.mkdir(path);
    return path;
  };
  const choose = async () => {
    if (!start()) return;
    try {
      const [picked] = await pick({
        allowMultiSelection: false,
      });
      if (!picked) return;
      const extension =
        picked.name?.match(/\.([a-zA-Z0-9]{1,10})$/)?.[1] ?? 'media';
      const [copy] = await keepLocalCopy({
        files: [
          {
            uri: picked.uri,
            fileName: `audio-input-${Date.now()}.${extension}`,
          },
        ],
        destination: 'cachesDirectory',
      });
      if (!copy || copy.status === 'error')
        throw new Error('Could not copy the selected file');
      const path = copy.localUri.replace('file://', '');
      if (source) await FileSystem.unlink(source.path).catch(() => {});
      setSource({ path, label: picked.name ?? 'Selected media' });
      setResult(null);
      setStatus('Ready to convert the first audio track.');
    } catch (value) {
      if (
        isErrorWithCode(value) &&
        value.code === errorCodes.OPERATION_CANCELED
      )
        setStatus('Selection canceled');
      else error(value);
    } finally {
      finish();
    }
  };
  const loadFixture = async (fixture: (typeof audioFixtures)[number]) => {
    if (!start()) return;
    try {
      const path = `${await directory()}/${fixture.filename}`;
      await FileSystem.writeFile(path, fixture.base64, 'base64');
      if (source && source.path !== path)
        await FileSystem.unlink(source.path).catch(() => {});
      setSource({ path, label: fixture.label, expected: fixture });
      setResult(null);
      setStatus('Sample ready. No conversion has run yet.');
    } catch (value) {
      error(value);
    } finally {
      finish();
    }
  };
  const convert = async () => {
    if (!source || !start()) return;
    try {
      setResult(null);
      setStatus('Converting on a native worker thread…');
      const destination = `${Dirs.CacheDir}/result.wav`;
      // This screen owns its cache result and replaces it on each conversion.
      if (await FileSystem.exists(destination))
        await FileSystem.unlink(destination);
      setOutputPath(null);
      const clock = (
        globalThis as typeof globalThis & { performance: { now(): number } }
      ).performance;
      const started = clock.now();
      const converted = await convertToWav(
        `file://${source.path
          .split('/')
          .map(segment => encodeURIComponent(segment))
          .join('/')}`,
        `file://${destination
          .split('/')
          .map(segment => encodeURIComponent(segment))
          .join('/')}`,
      );
      setElapsed(clock.now() - started);
      const previous = outputPath;
      setOutputPath(converted.path);
      setResult(converted);
      if (previous && previous !== converted.path)
        await FileSystem.unlink(previous).catch(() => {});
      const expected = source.expected;
      if (
        expected &&
        (converted.sampleRate !== expected.sampleRate ||
          converted.channels !== expected.channels ||
          Math.abs(converted.durationSeconds - expected.durationSeconds) > 0.15)
      ) {
        setStatus(
          'Converted, but sample format/duration differs from the fixture expectation.',
        );
      } else
        setStatus(
          expected
            ? 'Converted · sample format and duration check passed.'
            : 'Converted successfully.',
        );
    } catch (value) {
      error(value);
    } finally {
      finish();
    }
  };
  const clear = async () => {
    if (!start()) return;
    try {
      if (source) await FileSystem.unlink(source.path).catch(() => {});
      if (outputPath) await FileSystem.unlink(outputPath).catch(() => {});
      setSource(null);
      setOutputPath(null);
      setResult(null);
      setStatus('Temporary input and output cleared.');
    } finally {
      finish();
    }
  };
  return (
    <ScrollView contentContainerStyle={styles.content}>
      <Text style={styles.title}>Audio → WAV</Text>
      <Text style={styles.note}>
        Decode the first audio track to 16-bit PCM WAV, preserving its sample
        rate and mono/stereo channels. No file-extension filter is applied. The
        device decoder determines which audio formats and containers work.
      </Text>
      <Text style={styles.heading}>Generated test tones</Text>
      {audioFixtures.map(fixture => (
        <Action
          key={fixture.filename}
          title={fixture.label}
          disabled={busy}
          onPress={() => {
            void loadFixture(fixture);
          }}
        />
      ))}
      <Text style={styles.heading}>Your file</Text>
      <Action
        title="Choose media file"
        disabled={busy}
        onPress={() => {
          void choose();
        }}
      />
      <Text style={styles.body}>{source?.label ?? 'No input selected'}</Text>
      <Action
        title="Convert to WAV"
        disabled={busy || !source}
        onPress={() => {
          void convert();
        }}
      />
      {busy && <ActivityIndicator color="#80caff" />}
      <Text accessibilityLiveRegion="polite" style={styles.body}>
        {status}
      </Text>
      {result && (
        <View style={styles.card}>
          <Text style={styles.heading}>WAV output</Text>
          <Text style={styles.body}>
            {elapsed.toFixed(1)} ms · {result.durationSeconds.toFixed(3)} s
          </Text>
          <Text style={styles.body}>
            {result.sampleRate} Hz · {result.channels} channels · signed 16-bit
            PCM
          </Text>
          <Text style={styles.body}>
            {result.frames.toLocaleString()} frames ·{' '}
            {(result.bytes / 1024).toFixed(1)} KiB
          </Text>
          <Text selectable style={styles.note}>
            {result.uri}
          </Text>
        </View>
      )}
      <Action
        title="Clear temporary audio files"
        disabled={busy}
        onPress={() => {
          void clear();
        }}
      />
      <Text style={styles.note}>
        Conversion streams bounded buffers; no audio base64 crosses the
        JS/native boundary. Samples use base64 only to install small bundled
        fixtures. Outputs remain in app cache until cleared, replaced or evicted
        by the OS.
      </Text>
    </ScrollView>
  );
}
const styles = StyleSheet.create({
  content: { padding: 20, gap: 16 },
  title: { color: '#f0f6ff', fontSize: 28, fontWeight: '700' },
  heading: { color: '#e1edff', fontSize: 17, fontWeight: '600' },
  body: { color: '#d1deed', fontSize: 14, lineHeight: 22 },
  note: { color: '#a5b6cb', fontSize: 13, lineHeight: 20 },
  button: {
    borderWidth: 1,
    borderColor: '#42627f',
    borderRadius: 8,
    padding: 12,
  },
  buttonText: { color: '#e1edff', fontSize: 14 },
  disabled: { opacity: 0.4 },
  card: { backgroundColor: '#1b2838', padding: 16, borderRadius: 12, gap: 10 },
});
