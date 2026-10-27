import {
  errorCodes,
  isErrorWithCode,
  keepLocalCopy,
  pick,
} from '@react-native-documents/picker';
import {
  extractPngTextChunk,
  replacePngTextChunk,
} from '@vali98/react-native-png-utils';
import React, { useRef, useState } from 'react';
import {
  ActivityIndicator,
  ScrollView,
  StyleSheet,
  Text,
  TouchableOpacity,
  View,
} from 'react-native';
import { FileSystem } from 'react-native-file-access';
import { SafeAreaView } from 'react-native-safe-area-context';
import { base64Bytes, mib, pause, randomPNG, summary } from './benchmark';
import AudioScreen from './AudioScreen';

type Source = { name: string; base64: string; bytes: number };
type Result = {
  name: string;
  bytes: number;
  iterations: number;
  extract: ReturnType<typeof summary>;
  replace: ReturnType<typeof summary>;
  chunks: number;
  outputBytes: number;
  text: ReturnType<typeof extractPreview>;
};
const clock = (
  globalThis as typeof globalThis & { performance: { now(): number } }
).performance;
const marker = {
  keyword: 'png-utils-benchmark',
  data: 'PNG benchmark ✓',
  b64encode: true,
};
function Button({
  title,
  onPress,
  disabled = false,
  selected = false,
}: {
  title: string;
  onPress: () => void;
  disabled?: boolean;
  selected?: boolean;
}) {
  return (
    <TouchableOpacity
      accessibilityRole="button"
      accessibilityState={{ disabled, selected }}
      disabled={disabled}
      onPress={onPress}
      style={[
        styles.button,
        selected && styles.selected,
        disabled && styles.disabled,
      ]}
    >
      <Text style={styles.buttonText}>{title}</Text>
    </TouchableOpacity>
  );
}

// Keep full native results in this synchronous frame, not across renders/yields.
function sampleExtract(input: string) {
  const start = clock.now();
  const extracted = extractPngTextChunk(input);
  const time = clock.now() - start;
  return { time, chunks: extracted.length };
}
function sample(input: string) {
  const extracted = sampleExtract(input);
  const start = clock.now();
  const replaced = replacePngTextChunk(input, [marker]);
  const replace = clock.now() - start;
  return {
    extract: extracted.time,
    replace,
    chunks: extracted.chunks,
    outputBytes: base64Bytes(replaced),
  };
}
// Capture display text once outside the timed calls, with a per-result budget.
function extractPreview(input: string) {
  const chunks = extractPngTextChunk(input);
  const entries: { keyword: string; data: string; characters: number }[] = [];
  let remaining = 16384;
  for (const chunk of chunks) {
    if (entries.length >= 32 || remaining === 0) break;
    const data = chunk.data.slice(0, remaining);
    entries.push({
      keyword: chunk.keyword,
      data,
      characters: chunk.data.length,
    });
    remaining -= data.length;
  }
  return { entries, omitted: chunks.length - entries.length };
}
function verify(input: string) {
  const output = replacePngTextChunk(input, [marker]);
  const found = extractPngTextChunk(output, { keywords: [marker.keyword] });
  if (!found.some(chunk => chunk.data === marker.data))
    throw new Error('Metadata round-trip verification failed');
}
function PngBenchmarks(): React.JSX.Element {
  const [menu, setMenu] = useState<'manual' | 'random'>('manual');
  const [selectedName, setSelectedName] = useState('No image loaded');
  const [size, setSize] = useState(2);
  const [iterations, setIterations] = useState(10);
  const [busy, setBusy] = useState(false);
  const [status, setStatus] = useState('Ready');
  const [results, setResults] = useState<Result[]>([]);
  const loaded = useRef<Source | null>(null);
  const working = useRef(false);
  const begin = () => {
    if (working.current) return false;
    working.current = true;
    setBusy(true);
    return true;
  };
  const finish = () => {
    working.current = false;
    setBusy(false);
  };
  const showError = (error: unknown) =>
    setStatus(error instanceof Error ? error.message : String(error));

  const load = async () => {
    if (!begin()) return;
    let copiedPath: string | undefined;
    try {
      setStatus('Choose a PNG');
      const [picked] = await pick({
        allowMultiSelection: false,
        type: ['image/png'],
      });
      if (!picked) return;
      // Release the previous input before reading another full image.
      loaded.current = null;
      setSelectedName('Loading…');
      const [copy] = await keepLocalCopy({
        files: [
          { fileName: `png-benchmark-${Date.now()}.png`, uri: picked.uri },
        ],
        destination: 'cachesDirectory',
      });
      if (!copy || copy.status === 'error')
        throw new Error('Could not copy selected image');
      copiedPath = copy.localUri.replace('file://', '');
      const base64 = await FileSystem.readFile(copiedPath, 'base64');
      if (!base64.startsWith('iVBORw0KGgo'))
        throw new Error('The selected file is not a PNG');
      const bytes = base64Bytes(base64);
      const name = picked.name ?? 'Selected PNG';
      loaded.current = { name, base64, bytes };
      setSelectedName(`${name} · ${mib(bytes)}`);
      setStatus('Loaded. File reading is excluded from benchmark timings.');
    } catch (error) {
      setSelectedName(loaded.current?.name ?? 'No image loaded');
      if (
        isErrorWithCode(error) &&
        error.code === errorCodes.OPERATION_CANCELED
      )
        setStatus('Selection canceled');
      else showError(error);
    } finally {
      if (copiedPath) await FileSystem.unlink(copiedPath).catch(() => {});
      finish();
    }
  };
  const run = async () => {
    if (!begin()) return;
    try {
      let source: Source;
      if (menu === 'manual') {
        if (!loaded.current) throw new Error('Load a PNG first');
        source = loaded.current;
      } else {
        // Do not retain a manually loaded image alongside a generated image.
        loaded.current = null;
        setSelectedName('No image loaded');
        setStatus(`Generating ${size} MiB random PNG…`);
        await pause();
        const generated = await randomPNG(size);
        source = {
          name: `Random RGB ${generated.width}×${generated.height}`,
          ...generated,
        };
      }
      setStatus('Verifying metadata round trip and warming up…');
      await pause();
      verify(source.base64);
      sample(source.base64);
      const extract: number[] = [];
      const replace: number[] = [];
      let last: ReturnType<typeof sample> | undefined;
      for (let i = 0; i < iterations; i++) {
        setStatus(`Running ${i + 1}/${iterations} · ${mib(source.bytes)}`);
        await pause();
        last = sample(source.base64);
        extract.push(last.extract);
        replace.push(last.replace);
      }
      if (!last) return;
      const result: Result = {
        name: source.name,
        bytes: source.bytes,
        iterations,
        extract: summary(extract),
        replace: summary(replace),
        chunks: last.chunks,
        outputBytes: last.outputBytes,
        text: extractPreview(source.base64),
      };
      // Retain timing summaries and bounded text previews, never replacement images.
      setResults(previous => [result, ...previous].slice(0, 8));
      setStatus(
        'Complete · round-trip verified. Timing summaries and bounded text previews retained.',
      );
    } catch (error) {
      showError(error);
    } finally {
      finish();
    }
  };
  return (
    <View style={styles.container}>
      <ScrollView contentContainerStyle={styles.content}>
        <Text style={styles.title}>PNG benchmarks</Text>
        <Text style={styles.note}>
          Use a release build for representative timings. Calls run
          synchronously; each iteration yields before the next pair of calls.
        </Text>
        <View style={styles.row}>
          <Button
            title="Manual images"
            selected={menu === 'manual'}
            disabled={busy}
            onPress={() => setMenu('manual')}
          />
          <Button
            title="Random PNG"
            selected={menu === 'random'}
            disabled={busy}
            onPress={() => setMenu('random')}
          />
        </View>
        {menu === 'manual' ? (
          <View style={styles.card}>
            <Text style={styles.heading}>Load your PNG</Text>
            <Text style={styles.body}>{selectedName}</Text>
            <View style={styles.row}>
              <Button
                title="Choose PNG"
                disabled={busy}
                onPress={() => {
                  void load();
                }}
              />
              <Button
                title="Release image"
                disabled={busy}
                onPress={() => {
                  loaded.current = null;
                  setSelectedName('No image loaded');
                  setStatus('Input reference released');
                }}
              />
            </View>
          </View>
        ) : (
          <View style={styles.card}>
            <Text style={styles.heading}>Random pixel PNG</Text>
            <Text style={styles.note}>
              Valid RGB PNG with deterministic random pixels and uncompressed
              DEFLATE. Size is approximate. Generation is excluded from timings.
            </Text>
            <View style={styles.row}>
              {[1, 2, 3, 8, 16].map(value => (
                <Button
                  key={value}
                  title={`${value} MiB`}
                  selected={size === value}
                  disabled={busy}
                  onPress={() => setSize(value)}
                />
              ))}
            </View>
          </View>
        )}
        <Text style={styles.heading}>Measured iterations</Text>
        <View style={styles.row}>
          {[5, 10, 25].map(value => (
            <Button
              key={value}
              title={String(value)}
              selected={iterations === value}
              disabled={busy}
              onPress={() => setIterations(value)}
            />
          ))}
        </View>
        <Button
          title="Benchmark extract + replace"
          disabled={busy}
          onPress={() => {
            void run();
          }}
        />
        {busy && <ActivityIndicator color="#80caff" />}
        <Text accessibilityLiveRegion="polite" style={styles.status}>
          {status}
        </Text>
        <Text style={styles.note}>
          Timings include JS/native conversion and the native call. Loading,
          generation, verification and one warm-up are excluded. Actual peak RAM
          requires Android Studio Memory Profiler or Xcode Instruments; this
          screen does not measure RAM.
        </Text>
        <View style={styles.row}>
          <Text style={styles.heading}>Results</Text>
          <Button
            title="Clear results"
            disabled={busy}
            onPress={() => setResults([])}
          />
        </View>
        {results.map((result, index) => (
          <View style={styles.card} key={`${index}-${result.name}`}>
            <Text style={styles.heading}>{result.name}</Text>
            <Text style={styles.body}>
              {mib(result.bytes)} · {result.iterations} iterations ·{' '}
              {result.chunks} text chunks
            </Text>
            {(['extract', 'replace'] as const).map(operation => (
              <Text key={operation} style={styles.body}>
                {operation}: median {result[operation].median.toFixed(2)} ms ·
                p95 {result[operation].p95.toFixed(2)} ms · mean{' '}
                {result[operation].mean.toFixed(2)} ms
              </Text>
            ))}
            <Text style={styles.heading}>Extracted tEXt content</Text>
            <Text style={styles.note}>
              From the original image, with base64 values decoded when valid.
              Preview extraction is outside the timings. Up to 16,384 characters
              across 32 chunks are retained per result.
            </Text>
            {result.chunks === 0 && (
              <Text style={styles.body}>
                No tEXt chunks in the original PNG.
              </Text>
            )}
            {result.text.entries.map((chunk, chunkIndex) => (
              <View key={chunkIndex} style={styles.textChunk}>
                <Text style={styles.heading}>
                  {chunk.keyword || '(empty keyword)'}
                </Text>
                <Text selectable style={styles.extractedText}>
                  {chunk.data || '(empty value)'}
                </Text>
                {chunk.data.length < chunk.characters && (
                  <Text style={styles.note}>
                    Preview truncated: showing{' '}
                    {chunk.data.length.toLocaleString()}
                    {' of '}
                    {chunk.characters.toLocaleString()} characters.
                  </Text>
                )}
              </View>
            ))}
            {result.text.omitted > 0 && (
              <Text style={styles.note}>
                {result.text.omitted} additional chunks omitted from the
                preview.
              </Text>
            )}
            <Text style={styles.note}>
              Output: {mib(result.outputBytes)}. Each sample replaces the same
              original input; output does not grow across iterations.
            </Text>
            <Text style={styles.note}>
              RAM scale estimate, not a measurement: one binary input is{' '}
              {mib(result.bytes)}; one base64 string holds about{' '}
              {mib(Math.ceil(result.bytes / 3) * 4)} of ASCII content. Native
              replacement buffer content peaks around{' '}
              {mib((result.bytes * 7) / 3)}, excluding the incoming base64
              string, JS/native copies, metadata and allocator overhead.
            </Text>
          </View>
        ))}
      </ScrollView>
    </View>
  );
}
const styles = StyleSheet.create({
  container: { flex: 1, backgroundColor: '#101822' },
  content: { padding: 20, gap: 16 },
  title: { color: '#f0f6ff', fontSize: 28, fontWeight: '700' },
  heading: { color: '#e1edff', fontSize: 17, fontWeight: '600' },
  body: { color: '#d1deed', fontSize: 14, lineHeight: 22 },
  note: { color: '#a5b6cb', fontSize: 13, lineHeight: 20 },
  status: { color: '#80caff', lineHeight: 22 },
  textChunk: {
    backgroundColor: '#101822',
    padding: 12,
    borderRadius: 8,
    gap: 8,
  },
  extractedText: {
    color: '#d1deed',
    fontFamily: 'monospace',
    fontSize: 12,
    lineHeight: 18,
  },
  card: { backgroundColor: '#1b2838', padding: 16, borderRadius: 12, gap: 12 },
  row: { flexDirection: 'row', flexWrap: 'wrap', alignItems: 'center', gap: 8 },
  button: {
    borderWidth: 1,
    borderColor: '#42627f',
    borderRadius: 8,
    paddingHorizontal: 12,
    paddingVertical: 10,
  },
  selected: { backgroundColor: '#285c83', borderColor: '#80caff' },
  disabled: { opacity: 0.4 },
  buttonText: { color: '#e1edff', fontSize: 14, fontWeight: '600' },
});
function App(): React.JSX.Element {
  const [screen, setScreen] = useState<'png' | 'audio'>('png');
  return (
    <SafeAreaView style={styles.container} edges={['top', 'bottom']}>
      <View style={[styles.row, { paddingHorizontal: 20, paddingTop: 8 }]}>
        <Button
          title="PNG benchmarks"
          selected={screen === 'png'}
          onPress={() => setScreen('png')}
        />
        <Button
          title="Audio → WAV"
          selected={screen === 'audio'}
          onPress={() => setScreen('audio')}
        />
      </View>
      <View style={{ flex: 1, display: screen === 'png' ? 'flex' : 'none' }}>
        <PngBenchmarks />
      </View>
      <View style={{ flex: 1, display: screen === 'audio' ? 'flex' : 'none' }}>
        <AudioScreen />
      </View>
    </SafeAreaView>
  );
}
export default App;
