const express = require('express');
const { exec } = require('child_process');
const fs = require('fs');
const path = require('path');
require('dotenv').config();

const checkApiKey = require('./src/middleware/apiKey');
const rateLimiter = require('./src/middleware/rateLimiter');

const app = express();
app.use(express.json());

app.use(checkApiKey);
app.use('/generate', rateLimiter);

const PORT = process.env.PORT || 8080;

const HGT_DIR = path.join(__dirname, 'hgt_files');
const STL_DIR = path.join(__dirname, 'stl_files');

if (!fs.existsSync(HGT_DIR)) fs.mkdirSync(HGT_DIR, { recursive: true });
if (!fs.existsSync(STL_DIR)) fs.mkdirSync(STL_DIR, { recursive: true });

const cli = path.join(__dirname, 'terrain_cli');

app.post('/generate', async (req, res) => {
    try {
        // params
        const { nwLat, nwLng, seLat, seLng, format, resolution, zScale, baseHeight, waterDrop, rotation } = req.body;
        if (!nwLat || !nwLng || !seLat || !seLng || !zScale) {
            return res.status(400).json({ message: "Missing required parameters: nwLat, nwLng, seLat, seLng, zScale" });
        }

        console.log(`[${new Date().toISOString()}] POST /generate hit from ${req.ip}`);

        const terrainReq = {
            nwLat: parseFloat(nwLat),
            nwLng: parseFloat(nwLng),
            seLat: parseFloat(seLat),
            seLng: parseFloat(seLng),
            format,
            resolution,
            zScale: parseFloat(zScale),
            baseHeight: baseHeight != null ? parseFloat(baseHeight) : parseFloat(3.0),
            waterDrop: waterDrop != null ? parseFloat(waterDrop) : parseFloat(1.0),
            rotation: rotation != null ? parseFloat(rotation) : parseFloat(0.0)
        };

        const outputSTL = path.resolve(STL_DIR, `terrain_${Date.now()}.stl`);

        // Run terrain_cli using the HGT tile
        // for now, just use local hgt files
        // adding s3 fetching later
        // console.log('Generating terrain:', terrainReq);

        const cmd = `${cli} --bbox ${nwLat},${nwLng},${seLat},${seLng} --format rect --zscale ${zScale} --output ${outputSTL}`;

        await new Promise((resolve, reject) => {
            exec(cmd, (err, stdout, stderr) => {
                if (err) return reject(`stl generation failed: ${stderr}`);
                console.log(stdout);
                resolve();
            });
        });
        
        // res.download(outputSTL, err => {
        //     if (!err) fs.unlinkSync(outputSTL);
        // });

        // // Read STL file as binary
        const stlBuffer = fs.readFileSync(outputSTL);
        
        // Send binary STL in response
        res.setHeader('Content-Type', 'application/sla');
        res.setHeader('Content-Disposition', `attachment; filename="terrain.stl"`);
        res.status(200).send(stlBuffer);
        
        // Remove temp file after sending response
        fs.unlinkSync(outputSTL);

    } catch (err) {
        console.error("Error in microservice at endpoint /generate:", err);
        res.status(500).json(err);
    }
});

app.listen(PORT, () => console.log(`Terrain service running on port: ${PORT}\n`));
